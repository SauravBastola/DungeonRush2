// Systems.cpp — implementations for all gameplay systems.
//
// Conventions:
//   - The global `manager` and `lastFrameTime` are defined in main.cpp/Game.cpp.
//   - Entity references in lambdas go through `&entity` (the Entity* stored in
//     each Component).  The ECS does not expose the Manager via Entity.

#include "Systems.h"
#include "Collision.h"
#include "AssetManager.h"
#include "TextureManager.h"
#include "SpriteSpec.h"
#include "Vfx.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <SDL_mixer.h>

extern Manager manager;
extern float   lastFrameTime;

// ===========================================================================
// Physics
// ===========================================================================
namespace Physics {

MoveResult moveAndCollide(Entity& e, const std::vector<SDL_Rect>& solids) {
    MoveResult r;
    auto& tr  = e.getComponent<TransformComponent>();
    auto& col = e.getComponent<ColliderComponent>();
    // From here on physics owns this entity's position; TransformComponent
    // must stop free-integrating velocity on top of it.
    tr.physicsOwnsMotion = true;
    SDL_Rect box = col.collider;

    // Resolve each axis ONCE, against the nearest blocker.
    //
    // The old code re-resolved inside the loop, so a box overlapping N solids
    // got shoved N times — with the inclusive AABB this walked the hero all
    // the way to the left edge of the map. Picking the minimum translation
    // (nearest blocker) is both correct and order-independent.
    // X axis
    int dx = (int)std::round(tr.velocity.x * lastFrameTime);
    if (dx != 0) {
        box.x += dx;
        bool hit = false;
        int  resolved = 0;
        for (auto& s : solids) {
            if (!Collision::AABB(box, s)) continue;
            int cand = (dx > 0) ? (s.x - box.w) : (s.x + s.w);
            if (!hit)                                   { resolved = cand; hit = true; }
            else if (dx > 0) { if (cand < resolved) resolved = cand; }
            else             { if (cand > resolved) resolved = cand; }
        }
        if (hit) {
            box.x = resolved;
            tr.velocity.x = 0;
            if (dx > 0) r.hitRight = true; else r.hitLeft = true;
        }
    }
    // Y axis
    int dy = (int)std::round(tr.velocity.y * lastFrameTime);
    if (dy != 0) {
        box.y += dy;
        bool hit = false;
        int  resolved = 0;
        for (auto& s : solids) {
            if (!Collision::AABB(box, s)) continue;
            int cand = (dy > 0) ? (s.y - box.h) : (s.y + s.h);
            if (!hit)                                   { resolved = cand; hit = true; }
            else if (dy > 0) { if (cand < resolved) resolved = cand; }
            else             { if (cand > resolved) resolved = cand; }
        }
        if (hit) {
            box.y = resolved;
            tr.velocity.y = 0;
            if (dy > 0) r.grounded = true; else r.hitCeil = true;
        }
    }

    tr.position.x = box.x - col.xOffset;
    tr.position.y = box.y - col.yOffset;
    return r;
}

MoveResult moveAndCollideOneWay(Entity& e, const std::vector<SDL_Rect>& oneWays) {
    MoveResult r;
    auto& tr  = e.getComponent<TransformComponent>();
    auto& col = e.getComponent<ColliderComponent>();
    tr.physicsOwnsMotion = true;
    SDL_Rect box = col.collider;

    int dy = (int)std::round(tr.velocity.y * lastFrameTime);
    if (dy >= 0) {
        box.y += dy;
        SDL_Rect prev = col.collider;
        for (auto& s : oneWays) {
            if (!Collision::AABB(box, s)) continue;
            if (prev.y + prev.h <= s.y + 2) {
                box.y = s.y - box.h;
                r.grounded = true;
                tr.velocity.y = 0;
            }
        }
    } else {
        box.y += dy;
        for (auto& s : oneWays) {
            if (!Collision::AABB(box, s)) continue;
            box.y = s.y + s.h;
            r.hitCeil = true;
            tr.velocity.y = 0;
        }
    }
    int dx = (int)std::round(tr.velocity.x * lastFrameTime);
    box.x += dx;

    tr.position.x = box.x - col.xOffset;
    tr.position.y = box.y - col.yOffset;
    return r;
}

void stepPlatforms(std::vector<Entity*>& platforms) {
    for (auto* p : platforms) {
        if (p->hasComponent<MovingPlatformComponent>())
            p->getComponent<MovingPlatformComponent>().step();
    }
}

void applyPlatformCarry(Entity& player, const std::vector<Entity*>& platforms) {
    if (!player.hasComponent<ColliderComponent>()) return;
    SDL_Rect pb = player.getComponent<ColliderComponent>().collider;
    for (auto* p : platforms) {
        if (!p->hasComponent<ColliderComponent>() || !p->hasComponent<TransformComponent>()) continue;
        SDL_Rect plb = p->getComponent<ColliderComponent>().collider;
        bool standing = (pb.y + pb.h >= plb.y - 2) && (pb.y + pb.h <= plb.y + 14)
                     && (pb.x + pb.w > plb.x) && (pb.x < plb.x + plb.w);
        if (standing) {
            auto& pt = p->getComponent<TransformComponent>();
            player.getComponent<TransformComponent>().position.x += pt.velocity.x * lastFrameTime;
            player.getComponent<TransformComponent>().position.y += pt.velocity.y * lastFrameTime;
            player.getComponent<ColliderComponent>().update();
        }
    }
}

} // namespace Physics

// ===========================================================================
// Particles
// ===========================================================================
namespace Particles {

static Entity& makeParticle(float x, float y, float vx, float vy,
                            float ax, float ay, float life,
                            float startSize, float endSize,
                            Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255,
                            float drag = 1.2f) {
    auto& p = manager.addEntity("particle");
    p.addComponent<TransformComponent>(x, y, 4, 4, 1.f);
    auto& pc = p.addComponent<ParticleComponent>();
    // Register with the particle group.  Without this the entity is in no
    // group at all, so Particles::step() never ticks it (life never runs out
    // -> every particle ever emitted leaks) and Particles::draw() never sees
    // it -> landing dust, hit sparks, death bursts and pickups were all
    // completely invisible.
    p.addGroup(Game::groupParticles);
    pc.life = pc.lifeMax = life;
    pc.vx = vx; pc.vy = vy; pc.ax = ax; pc.ay = ay;
    pc.startSize = startSize; pc.endSize = endSize;
    pc.r = r; pc.g = g; pc.b = b; pc.a = a;
    pc.drag = drag;
    return p;
}

void emitDust(float x, float y, Uint8 r, Uint8 g, Uint8 b) {
    for (int i = 0; i < 6; i++) {
        float a = (rand() % 360) * 3.1415f / 180.f;
        float s = 30.f + rand() % 50;
        makeParticle(x, y, std::cos(a)*s, std::sin(a)*s - 20.f,
                     0, 80.f, 0.45f, 4, 1, r, g, b);
    }
}

void emitHitSparks(float x, float y) {
    for (int i = 0; i < 8; i++) {
        float a = (rand() % 360) * 3.1415f / 180.f;
        float s = 80.f + rand() % 120;
        makeParticle(x, y, std::cos(a)*s, std::sin(a)*s,
                     0, 200.f, 0.30f, 3, 0, 255, 230, 120, 255, 4.f);
    }
}

void emitDeathBurst(float x, float y, Uint8 r, Uint8 g, Uint8 b) {
    for (int i = 0; i < 24; i++) {
        float a = (rand() % 360) * 3.1415f / 180.f;
        float s = 60.f + rand() % 200;
        makeParticle(x, y, std::cos(a)*s, std::sin(a)*s,
                     0, 200.f, 0.7f + (rand()%100)/200.f, 5, 0,
                     r, g, b);
    }
}

void emitHeal(float x, float y) {
    for (int i = 0; i < 14; i++) {
        float a = (rand() % 360) * 3.1415f / 180.f;
        float s = 20.f + rand() % 50;
        makeParticle(x, y, std::cos(a)*s, std::sin(a)*s - 60.f,
                     0, 60.f, 0.7f, 3, 0, 120, 255, 160);
    }
}

void step(std::vector<Entity*>& particles) {
    for (auto* p : particles) {
        if (p->isActive() && p->hasComponent<ParticleComponent>())
            p->getComponent<ParticleComponent>().update();
    }
}

void draw(std::vector<Entity*>& particles) {
    SDL_Renderer* r = Game::renderer;
    for (auto* p : particles) {
        if (!p->isActive() || !p->hasComponent<ParticleComponent>()) continue;
        auto& pc = p->getComponent<ParticleComponent>();
        auto& tr = p->getComponent<TransformComponent>();
        float sz = pc.size();
        float a  = pc.alpha();
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, pc.r, pc.g, pc.b, (Uint8)std::min(255.f, a));
        SDL_Rect d{(int)(tr.position.x - sz/2 - Game::camera.x),
                   (int)(tr.position.y - sz/2 - Game::camera.y),
                   (int)sz, (int)sz};
        SDL_RenderFillRect(r, &d);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

} // namespace Particles

// ===========================================================================
// Damage — the single funnel every source of harm to the hero passes through
// ===========================================================================
namespace Damage {

bool applyToPlayer(Entity& player, float amount, int& shieldCharges,
                   float invincT, float& iframeT, float& shake) {
    auto& hp = player.getComponent<HealthComponent>();
    if (hp.dead) return false;

    // invincibility powerup / dash / respawn grace: no interaction at all
    if (invincT > 0.f || hp.iframes > 0.f) return false;

    // a shield charge eats the hit instead of HP
    if (shieldCharges > 0) {
        shieldCharges--;
        iframeT = Config::IFRAME_TIME;
        hp.iframes = Config::IFRAME_TIME;
        shake += 6.f;
        if (auto* s = Game::assets->GetSound("invincible")) Mix_PlayChannel(-1, s, 0);
        auto& tr = player.getComponent<TransformComponent>();
        Particles::emitDust(tr.position.x + tr.width / 2.f,
                            tr.position.y + tr.height / 2.f, 150, 220, 255);
        return false;
    }

    hp.damage(amount);
    iframeT = Config::IFRAME_TIME;
    shake += 8.f;
    if (auto* s = Game::assets->GetSound("hit")) Mix_PlayChannel(-1, s, 0);
    return true;
}

} // namespace Damage

// ===========================================================================
// Combat
// ===========================================================================
namespace Combat {

// Melee and ranged used to be welded to the same key: pressing J spawned a
// close-range hitbox AND fired a projectile on the third combo step, so the
// hero swung a sword and shot a bolt in one input.  They are separate moves
// now — see Config::ATTACKS and Game::handleAttacks().
//
// `def` is the row straight out of that table, so every move is tuned in one
// place and this function never grows another special case.
void spawnMeleeHitbox(Entity& owner, const Config::AttackDef& def, int dir,
                      float damage, int kind) {
    auto& hb = manager.addEntity("hitbox");
    auto& tr = owner.getComponent<TransformComponent>();
    const int w = def.boxW, h = def.boxH;
    hb.addComponent<TransformComponent>(tr.position.x, tr.position.y, w, h, 1.f);
    auto& col = hb.addComponent<ColliderComponent>("hitbox", 0, 0, w, h);
    col.sensor = true; col.solid = false;
    auto& hc = hb.addComponent<HitboxComponent>();
    hc.owner    = &owner;
    hc.damage   = damage;
    hc.knockX   = def.knockX * dir;
    hc.knockY   = def.knockY;
    hc.ttl      = (int)(def.duration * Config::FPS);
    hc.width    = w;
    hc.height   = h;
    hc.xOffset  = def.offX;
    hc.yOffset  = def.offY;
    hc.centred  = def.centred;
    hc.dir      = dir;
    hc.tag      = "player";
    hc.bounce   = def.bounce;
    hc.stunT    = def.stun;
    hc.kind     = kind;

    // The group is not cosmetic — Combat::resolveMelee() is handed
    // manager.getGroup(Game::groupHitboxes) as its only source of hitboxes.
    // Without addGroup() this entity sits in no group at all, the attack
    // swings through thin air for zero damage, and the per-level cleanup sweep
    // in Game::changeLevel() never finds it to destroy.
    hb.addGroup(Game::groupHitboxes);
}

// A player bolt.  Heavy bolts are bigger, faster, hit harder and PIERCE —
// they are not consumed by the first enemy they touch.
void spawnPlayerBolt(Entity& owner, int dir, bool heavy) {
    const float speed  = heavy ? (float)Config::HEAVY_SPEED : (float)Config::BOLT_SPEED;
    const float damage = heavy ? Config::ATTACKS[(int)Config::AttackKind::HeavyBolt].dmg
                               : Config::ATTACKS[(int)Config::AttackKind::Bolt].dmg;
    const float life   = heavy ? Config::HEAVY_LIFE : Config::BOLT_LIFE;
    auto& pr = manager.addEntity("projectile");
    auto& tr = owner.getComponent<TransformComponent>();
    const float s = heavy ? Config::HEAVY_RADIUS_SCALE : Config::BOLT_RADIUS_SCALE;
    const int w = (int)(20 * s), h = (int)(20 * s);
    float px = tr.position.x + tr.width / 2.f - w / 2.f + dir * (tr.width / 2.f + 6);
    float py = tr.position.y + tr.height * 0.34f - h / 2.f;
    pr.addComponent<TransformComponent>(px, py, w, h, 1.f);

    auto& sp = pr.addComponent<SpriteComponent>(
        Sprites::FIREBALL.tex, Sprites::FIREBALL.fw, Sprites::FIREBALL.fh,
        Sprites::FIREBALL.scale * s, true);
    Sprites::apply(sp, Sprites::FIREBALL);
    sp.addAnim("spin", Animation(0, 1, 100));
    sp.Play("spin");
    if (heavy) sp.setTint(150, 210, 255);

    auto& col = pr.addComponent<ColliderComponent>("projectile", 0, 0, w, h);
    col.sensor = true; col.solid = false;
    auto& pc = pr.addComponent<ProjectileComponent>(
        Vector2D((float)dir * speed, 0.f), damage, "player", &owner, life);
    pc.pierce = heavy;
    pc.stunT  = heavy ? 0.30f : 0.14f;
    pr.addGroup(Game::groupProjectiles);

    Vfx::sparkle(px + w / 2.f, py + h / 2.f, heavy ? 10 : 5);
}

Entity& spawnProjectile(Entity& owner, Vector2D vel, float damage, const std::string& tag,
                     const std::string& tex, float life) {
    auto& pr = manager.addEntity("projectile");
    auto& tr = owner.getComponent<TransformComponent>();
    int w = 16, h = 16;
    float px = tr.position.x + tr.width/2.f - w/2.f;
    float py = tr.position.y + tr.height/2.f - h/2.f;
    pr.addComponent<TransformComponent>(px, py, w, h, 1.f);
    auto& sp = pr.addComponent<SpriteComponent>(
        tex, Sprites::FIREBALL.fw, Sprites::FIREBALL.fh,
        Sprites::FIREBALL.scale, true);
    Sprites::apply(sp, Sprites::FIREBALL);
    sp.addAnim("spin", Animation(0, 1, 100));
    sp.Play("spin");
    if (tag == "player") {
        sp.frameW = 32; sp.frameH = 32; sp.scale = 0.5f;
        sp.setTex("projectile");
    }
    auto& col = pr.addComponent<ColliderComponent>("projectile", 0, 0, w, h);
    col.sensor = true; col.solid = false;
    pr.addComponent<ProjectileComponent>(vel, damage, tag, &owner, life);
    pr.addGroup(Game::groupProjectiles);
    return pr;                              // Batch C4: callers can adjust it (gravity)
}

void spawnShockwave(Entity& owner, int dir, float damage) {
    auto& sw = manager.addEntity("shockwave");
    auto& tr = owner.getComponent<TransformComponent>();
    float px = tr.position.x + (dir > 0 ? tr.width + 4 : -36);
    float py = tr.position.y + tr.height - 32;   // hug the floor
    sw.addComponent<TransformComponent>(px, py, 32, 32, 1.f);
    auto& sp = sw.addComponent<SpriteComponent>(
        Sprites::SHOCKWAVE.tex, Sprites::SHOCKWAVE.fw, Sprites::SHOCKWAVE.fh,
        Sprites::SHOCKWAVE.scale, true);
    Sprites::apply(sp, Sprites::SHOCKWAVE);
    sp.addAnim("roll", Animation(0, 4, 60));
    sp.Play("roll");
    auto& col = sw.addComponent<ColliderComponent>("shockwave", 0, 0, 32, 32);
    col.sensor = true; col.solid = false;
    sw.addComponent<ProjectileComponent>(
        Vector2D((float)(dir * Config::BOSS_SHOCKWAVE_SPEED), 0.f),
        damage, "enemy", &owner, Config::BOSS_SHOCKWAVE_LIFE);
    sw.addGroup(Game::groupProjectiles);
}

void spawnProjectileFan(Entity& owner, Entity& target, int count,
                        float spread, float speed, float damage) {
    auto& ot = owner.getComponent<TransformComponent>();
    auto& tt = target.getComponent<TransformComponent>();
    float dx = tt.position.x - ot.position.x;
    float dy = (tt.position.y - tt.height * 0.5f) - (ot.position.y - ot.height * 0.5f);
    float base = std::atan2(dy, dx);
    for (int i = 0; i < count; i++) {
        // centre the fan on `base`
        float off = (count == 1) ? 0.f
                  : (spread * ((float)i / (count - 1) - 0.5f));
        float a = base + off;
        Vector2D v(std::cos(a) * speed, std::sin(a) * speed);
        spawnProjectile(owner, v, damage, "enemy", "fireball", 2.4f);
    }
}

// Returns how many victims were struck this frame, so the caller can drive
// the hit-combo counter and the freeze-frame hit-stop.
int resolveMelee(std::vector<Entity*>& hitboxes,
                 std::vector<Entity*>& targets,
                 float& screenShake,
                 std::vector<Config::HitEvent>* events) {
    int landed = 0;
    for (auto* hb : hitboxes) {
        if (!hb->isActive() || !hb->hasComponent<HitboxComponent>()) continue;
        auto& hc = hb->getComponent<HitboxComponent>();
        if (!hc.owner || !hc.owner->isActive()) continue;
        for (auto* t : targets) {
            if (!t->isActive() || !t->hasComponent<ColliderComponent>() || !t->hasComponent<HealthComponent>())
                continue;
            auto& col = t->getComponent<ColliderComponent>();
            if (!Collision::AABB(hc.box, col.collider)) continue;
            std::uintptr_t id = reinterpret_cast<std::uintptr_t>(t);
            if (hc.hit.count(id)) continue;
            hc.hit.insert(id);
            // Batch C: a guarding skeleton blocks a blow from the side it faces
            if (hc.owner->hasComponent<TransformComponent>()) {
                auto& ot = hc.owner->getComponent<TransformComponent>();
                if (EnemyAI::blocks(*t, ot.position.x + ot.width * 0.5f)) {
                    auto& tt = t->getComponent<TransformComponent>();
                    Particles::emitHitSparks(tt.position.x + tt.width * 0.5f, tt.position.y + 20.f);
                    if (auto* s = Game::assets->GetSound("invincible")) Mix_PlayChannel(-1, s, 0);
                    ot.velocity.x += (ot.position.x < tt.position.x ? -1.f : 1.f) * 180.f;
                    continue;
                }
            }
            hc.hitCount++;
            landed++;
            auto& hp = t->getComponent<HealthComponent>();
            // damage() reports "the hit landed", not "the victim died" — a
            // landed hit on a live enemy returns true too.  Only a landed hit
            // that leaves hp.dead is a kill.
            const bool connected = hp.damage(hc.damage);
            const bool died      = connected && hp.dead;
            auto& tr  = t->getComponent<TransformComponent>();
            tr.velocity.x += hc.knockX;
            tr.velocity.y += hc.knockY;
            // Stagger the victim so the knockback is visible.  The boss gets a
            // much shorter window — it should shrug most of it off.
            if (t->hasComponent<EnemyAIComponent>()) {
                auto& ai = t->getComponent<EnemyAIComponent>();
                ai.hurtT = std::max(ai.hurtT,
                    (ai.kind == EnemyAIComponent::Kind::Boss) ? hc.stunT * 0.25f
                                                              : hc.stunT);
            }
            if (t->hasComponent<SpriteComponent>()) {
                auto& sp = t->getComponent<SpriteComponent>();
                sp.flash(0.12f);                 // red pop on the struck frame
                sp.squash(1.22f, 0.80f);         // and a recoil
            }
            float cx = tr.position.x + tr.width/2.f;
            float cy = tr.position.y + tr.height/2.f;
            Particles::emitHitSparks(cx, cy);
            Vfx::impactRing(cx, cy, 255, 236, 190, died ? 58.f : 40.f, 0.24f);
            if (died) {
                Particles::emitDeathBurst(cx, cy, 200, 80, 80);
                if (auto* s = Game::assets->GetSound("enemyDie")) Mix_PlayChannel(-1, s, 0);
            } else {
                if (auto* s = Game::assets->GetSound("hit")) Mix_PlayChannel(-1, s, 0);
            }
            screenShake += died ? 8.f : 4.f;
            if (events) events->push_back({ cx, cy, (int)hc.damage, died });

            // A connecting dive-slash kicks the hero back up, so it doubles as
            // a mobility move instead of a one-way ticket into a spike pit.
            if (hc.bounce != 0.f && hc.owner && hc.owner->isActive()
                && hc.owner->hasComponent<TransformComponent>()) {
                hc.owner->getComponent<TransformComponent>().velocity.y = hc.bounce;
            }
        }
    }
    return landed;
}

} // namespace Combat

// ===========================================================================
// Enemy AI
// ===========================================================================
namespace EnemyAI {

// A bomber's fuse has run out.  Anything inside the blast radius takes the
// hit — including the hero and any other bombers, which chain-detonate.
static void detonate(Entity& bomber, Entity& player,
                     const DamageSink& sink, float& screenShake) {
    auto& bt = bomber.getComponent<TransformComponent>();
    float bx = bt.position.x + bt.width  / 2.f;
    float by = bt.position.y + bt.height / 2.f;

    Particles::emitDeathBurst(bx, by, 255, 170, 60);
    Particles::emitDeathBurst(bx, by, 255, 90, 40);
    screenShake += 16.f;
    if (auto* s = Game::assets->GetSound("enemyDie")) Mix_PlayChannel(-1, s, 0);

    if (player.hasComponent<ColliderComponent>()) {
        auto& pc = player.getComponent<ColliderComponent>().collider;
        float cx = pc.x + pc.w / 2.f, cy = pc.y + pc.h / 2.f;
        float dx = cx - bx, dy = cy - by;
        if (dx*dx + dy*dy <= Config::BOMBER_BLAST_RADIUS * Config::BOMBER_BLAST_RADIUS) {
            if (Damage::applyToPlayer(player, Config::BOMBER_DAMAGE,
                                      sink.shieldCharges, sink.invincT,
                                      sink.iframeT, screenShake)) {
                auto& pt = player.getComponent<TransformComponent>();
                float d = std::sqrt(dx*dx + dy*dy) + 0.001f;
                pt.velocity.x += (dx / d) * 320.f;
                pt.velocity.y = std::min(pt.velocity.y - 180.f, -260.f);
            }
        }
    }

    // chain reaction: light any other bomber caught in the blast
    const float r2 = Config::BOMBER_BLAST_RADIUS * Config::BOMBER_BLAST_RADIUS;
    for (auto* other : manager.getGroup(Game::groupEnemies)) {
        if (other == &bomber || !other->isActive()) continue;
        if (!other->hasComponent<EnemyAIComponent>()) continue;
        auto& oai = other->getComponent<EnemyAIComponent>();
        if (oai.kind != EnemyAIComponent::Kind::Bomber) continue;
        auto& ot = other->getComponent<TransformComponent>();
        float ox = ot.position.x + ot.width  / 2.f - bx;
        float oy = ot.position.y + ot.height / 2.f - by;
        if (ox*ox + oy*oy <= r2 && oai.fuseT < 0.f) oai.fuseT = 0.18f;
    }

    // die like any other kill so the scorer picks it up
    if (bomber.hasComponent<HealthComponent>()) {
        auto& hp = bomber.getComponent<HealthComponent>();
        hp.hp = 0.f; hp.dead = true; hp.iframes = 0.f;
    }
}

// The boss has landed from its jump slam — throw a shockwave along the floor
// in both directions.  The only answer is to be airborne.
static void slamLanding(Entity& boss, float& screenShake) {
    auto& bt = boss.getComponent<TransformComponent>();
    float bx = bt.position.x + bt.width / 2.f;
    float by = bt.position.y + bt.height;

    Combat::spawnShockwave(boss, -1, Config::BOSS_SHOCKWAVE_DAMAGE);
    Combat::spawnShockwave(boss,  1, Config::BOSS_SHOCKWAVE_DAMAGE);

    Particles::emitDust(bx, by, 210, 190, 150);
    Particles::emitDust(bx, by, 170, 150, 120);
    Vfx::impactRing(bx, by - 8.f, 255, 210, 140, 130.f, 0.40f);
    Vfx::landDust(bx, by, 2.2f);
    // The boss sheet is a single frame, so squash is the only way it can
    // show the impact of its own landing.
    if (boss.hasComponent<SpriteComponent>())
        boss.getComponent<SpriteComponent>().squash(1.35f, 0.62f);
    screenShake += 18.f;
    if (auto* s = Game::assets->GetSound("hit")) Mix_PlayChannel(-1, s, 0);
}

void step(std::vector<Entity*>& enemies, Entity& player,
          const DamageSink& sink, float& screenShake) {
    for (auto* e : enemies) {
        if (!e->isActive() || !e->hasComponent<EnemyAIComponent>()) continue;
        auto& ai = e->getComponent<EnemyAIComponent>();
        auto& tr = e->getComponent<TransformComponent>();
        auto& hp = e->getComponent<HealthComponent>();
        if (hp.dead) {
            tr.velocity.Zero();
            if (!e->hasComponent<SpriteComponent>() ||
                e->getComponent<SpriteComponent>().currentAnim != "die") {
                if (e->hasComponent<SpriteComponent>()) {
                    auto& sp = e->getComponent<SpriteComponent>();
                    sp.Play("die");
                    sp.blinkT = 0.f;
                }
            }
            // destroy after a short delay handled by Game logic
            continue;
        }
        if (!ai.target) ai.target = &player;
        auto& pt = ai.target->getComponent<TransformComponent>();
        float dx = tr.position.x - pt.position.x;
        float adx = std::abs(dx);
        float dy  = tr.position.y - pt.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);

        // Batch C: no ai.update() here - Entity::update() already runs every
        // component's update() each frame, so calling it again ticked attackCd,
        // fireTimer and stateT twice: archers and mages fired at double their
        // configured rate.

        // Staggered: the knockback owns the body for a moment.  Without this
        // the switch below reassigns tr.velocity.x every single frame and the
        // hit reads as a no-op.
        if (ai.hurtT > 0.f) {
            ai.hurtT -= lastFrameTime;
            if (e->hasComponent<SpriteComponent>()) {
                auto& sp = e->getComponent<SpriteComponent>();
                // sheets name this row "hurt" (skeleton) or "hit"; take
                // whichever exists, fall back to a tint flash.
                if      (sp.anims.count("hurt")) sp.Play("hurt");
                else if (sp.anims.count("hit"))  sp.Play("hit");
                else                             sp.flash(0.08f);
            }
            continue;
        }

        switch (ai.kind) {
            case EnemyAIComponent::Kind::Skeleton: {
                // Batch C: tell -> strike -> guard, then back to the chase
                auto* sp = e->hasComponent<SpriteComponent>() ? &e->getComponent<SpriteComponent>() : nullptr;
                auto& ec = e->getComponent<ColliderComponent>().collider;
                auto& pc = player.getComponent<ColliderComponent>().collider;
                const float gap = (dx > 0) ? (float)(ec.x - (pc.x + pc.w)) : (float)(pc.x - (ec.x + ec.w));
                if (ai.swingPhase == 0 && gap < Config::SKEL_SWING_RANGE && std::fabs(dy) < 40.f &&
                    ai.attackCd <= 0.f) {
                    ai.swingPhase = 1; ai.swingT = 0.f;
                    tr.facingRight = dx < 0;
                }
                if (ai.swingPhase > 0) {
                    ai.swingT += lastFrameTime;
                    tr.velocity.x *= 0.6f;
                    if (ai.swingPhase == 1) {                                  // the tell
                        if (sp) sp->Play("windup");
                        if (ai.swingT >= Config::SKEL_WINDUP) { ai.swingPhase = 2; ai.swingT = 0.f; }
                    } else if (ai.swingPhase == 2) {                           // the strike
                        if (sp) sp->Play("strike");
                        const int dir = tr.facingRight ? 1 : -1;
                        SDL_Rect blade{ dir > 0 ? ec.x + ec.w : ec.x - (int)Config::SKEL_REACH,
                                        ec.y + 4, (int)Config::SKEL_REACH, ec.h - 8 };
                        if (Collision::AABB(blade, pc)) {
                            if (sink.parryT > 0.f) {                         // parried
                                ai.hurtT = Config::PARRY_STUN;
                                tr.velocity.x = -dir * 260.f;
                                ai.swingPhase = 0; ai.attackCd = Config::SKEL_SWING_CD;
                                sink.parryEvents++;
                                break;
                            }
                            auto& ptr = player.getComponent<TransformComponent>();
                            if (Damage::applyToPlayer(player, Config::SKEL_SWING_DAMAGE, sink.shieldCharges,
                                                      sink.invincT, sink.iframeT, screenShake)) {
                                ptr.velocity.x = dir * 300.f;
                                ptr.velocity.y = -280.f;
                            }
                        }
                        if (ai.swingT >= Config::SKEL_STRIKE) { ai.swingPhase = 3; ai.swingT = 0.f; }
                    } else {                                                   // guard
                        ai.guarding = true;
                        if (sp) sp->Play("guard");
                        if (ai.swingT >= Config::SKEL_GUARD) {
                            ai.swingPhase = 0; ai.guarding = false; ai.attackCd = Config::SKEL_SWING_CD;
                        }
                    }
                    break;
                }
                ai.guarding = false;
                if (adx < Config::ENEMY_CHASE_RANGE) {
                    ai.transition(EnemyAIComponent::State::Chase);
                    float dir = (dx > 0) ? -1.f : 1.f;
                    tr.velocity.x = dir * Config::ENEMY_CHASE_SPEED;
                    tr.facingRight = dir > 0;
                } else {
                    ai.transition(EnemyAIComponent::State::Patrol);
                    if (std::abs(tr.position.x - ai.patrolCenter) > Config::ENEMY_PATROL_R)
                        ai.patrolDir = (tr.position.x > ai.patrolCenter) ? -1 : 1;
                    tr.velocity.x = ai.patrolDir * 60.f;
                    tr.facingRight = ai.patrolDir > 0;
                }
                if (e->hasComponent<SpriteComponent>())
                    e->getComponent<SpriteComponent>().Play(
                        tr.velocity.x == 0 ? "idle" : "run");
                break;
            }
            case EnemyAIComponent::Kind::Slime: {
                ai.jumpTimer -= lastFrameTime;
                if (ai.jumpTimer <= 0) {
                    int dir = (dx > 0) ? -1 : 1;
                    tr.velocity.x = dir * 110.f;
                    tr.velocity.y = Config::SLIME_JUMP_VEL;
                    ai.jumpTimer = Config::SLIME_JUMP_PERIOD;
                    tr.facingRight = dir > 0;
                } else {
                    tr.velocity.x *= 0.92f;
                }
                if (e->hasComponent<SpriteComponent>())
                    e->getComponent<SpriteComponent>().Play(
                        tr.velocity.y < 0 ? "jump" : "idle");
                break;
            }
            case EnemyAIComponent::Kind::Spitter: {
                // Batch C4: rooted.  tell (pod swells, mouth glows) -> spit a seed
                // on a real arc at where the hero stands -> cooldown.
                auto* sp = e->hasComponent<SpriteComponent>() ? &e->getComponent<SpriteComponent>() : nullptr;
                tr.velocity.x = 0.f;
                if (ai.swingPhase == 0) {
                    tr.facingRight = dx < 0;
                    if (adx < Config::SPITTER_RANGE && std::fabs(dy) < 320.f && ai.attackCd <= 0.f) {
                        ai.swingPhase = 1; ai.swingT = 0.f;
                    }
                    if (sp) sp->Play("idle");
                } else if (ai.swingPhase == 1) {
                    ai.swingT += lastFrameTime;
                    if (sp) sp->Play("tell");
                    if (ai.swingT >= Config::SPITTER_TELL) {
                        const int dir = tr.facingRight ? 1 : -1;
                        const float sx = tr.position.x + (dir > 0 ? tr.width : 0.f);
                        const float sy = tr.position.y + 10.f;
                        const float tx = pt.position.x + 16.f, ty = pt.position.y + 25.f;
                        const float t = std::max(0.35f, std::min(1.6f, std::fabs(tx - sx) / Config::SPITTER_SEED_VX));
                        const float vx = (tx - sx) / t;
                        const float vy = (ty - sy - 0.5f * Config::PLAYER_GRAVITY * t * t) / t;
                        auto& seed = Combat::spawnProjectile(*e, Vector2D(vx, vy), Config::SPITTER_DAMAGE,
                                                             "enemy", "fireball", 3.0f);
                        seed.getComponent<ProjectileComponent>().gravity = true;
                        ai.swingPhase = 2; ai.swingT = 0.f;
                    }
                } else {
                    ai.swingT += lastFrameTime;
                    if (sp) sp->Play("spit");
                    if (ai.swingT >= 0.25f) { ai.swingPhase = 0; ai.attackCd = Config::SPITTER_COOLDOWN; }
                }
                break;
            }
            case EnemyAIComponent::Kind::Wisp: {
                // Batch C4: drifts through walls toward the hero; flares (tell),
                // lunges at where the hero was, hurts only during the lunge.
                auto* sp = e->hasComponent<SpriteComponent>() ? &e->getComponent<SpriteComponent>() : nullptr;
                ai.sinT += lastFrameTime;
                ai.diveCd -= lastFrameTime;
                const float hx = pt.position.x + 16.f - (tr.position.x + tr.width * 0.5f);
                const float hy = pt.position.y + 25.f - (tr.position.y + tr.height * 0.5f);
                const float hd = std::max(1.f, std::sqrt(hx * hx + hy * hy));
                if (ai.divePhase == 0) {
                    if (hd < Config::WISP_SENSE) {
                        tr.velocity.x = hx / hd * Config::WISP_DRIFT;
                        tr.velocity.y = hy / hd * Config::WISP_DRIFT + std::sin(ai.sinT * 3.f) * 20.f;
                    } else {
                        tr.velocity.x = 0.f;
                        tr.velocity.y = std::sin(ai.sinT * 2.f) * 20.f;
                    }
                    tr.facingRight = hx > 0;
                    if (hd < Config::WISP_LUNGE_RANGE && ai.diveCd <= 0.f) { ai.divePhase = 1; ai.diveT = 0.f; }
                    if (sp) sp->Play("float");
                } else if (ai.divePhase == 1) {                                // the tell
                    ai.diveT += lastFrameTime;
                    tr.velocity.x = 0.f; tr.velocity.y = 0.f;
                    if (sp) sp->Play("tell");
                    if (ai.diveT >= Config::WISP_TELL) {
                        ai.diveVX = hx / hd * Config::WISP_LUNGE_SPEED;
                        ai.diveVY = hy / hd * Config::WISP_LUNGE_SPEED;
                        ai.divePhase = 2; ai.diveT = 0.f;
                    }
                } else if (ai.divePhase == 2) {                                // the lunge
                    ai.diveT += lastFrameTime;
                    tr.velocity.x = ai.diveVX; tr.velocity.y = ai.diveVY;
                    tr.facingRight = ai.diveVX >= 0;
                    if (sp) sp->Play("lunge");
                    auto& ec = e->getComponent<ColliderComponent>().collider;
                    auto& pc = player.getComponent<ColliderComponent>().collider;
                    bool done = ai.diveT >= Config::WISP_LUNGE_TIME;
                    if (Collision::AABB(ec, pc)) {
                        if (Damage::applyToPlayer(player, Config::WISP_DAMAGE, sink.shieldCharges,
                                                  sink.invincT, sink.iframeT, screenShake)) {
                            auto& ptr = player.getComponent<TransformComponent>();
                            ptr.velocity.x += (ai.diveVX >= 0 ? 1.f : -1.f) * 200.f;
                        }
                        done = true;
                    }
                    if (done) { ai.divePhase = 0; ai.diveCd = Config::WISP_COOLDOWN; }
                }
                break;
            }
            case EnemyAIComponent::Kind::Bat: {
                ai.sinT += lastFrameTime;
                // Batch C: hover (tell) -> dive at where you were -> climb back
                if (ai.homeY < 0.f) ai.homeY = tr.position.y;
                ai.diveCd -= lastFrameTime;
                if (ai.divePhase == 0 && ai.diveCd <= 0.f && adx < Config::BAT_DIVE_RANGE_X &&
                    pt.position.y > tr.position.y + 40.f) {
                    ai.divePhase = 1; ai.diveT = 0.f;
                }
                if (ai.divePhase == 1) {                                       // the tell
                    ai.diveT += lastFrameTime;
                    tr.velocity.x = 0.f;
                    tr.velocity.y = std::sin(ai.sinT * 40.f) * 40.f;          // a hard flutter
                    tr.facingRight = dx < 0;
                    if (ai.diveT >= Config::BAT_DIVE_TELL) {
                        const float tx = pt.position.x + 16.f - (tr.position.x + 16.f);
                        const float ty = pt.position.y + 25.f - (tr.position.y + 16.f);
                        const float l = std::max(1.f, std::sqrt(tx * tx + ty * ty));
                        ai.diveVX = tx / l * Config::BAT_DIVE_SPEED;
                        ai.diveVY = ty / l * Config::BAT_DIVE_SPEED;
                        ai.divePhase = 2; ai.diveT = 0.f;
                    }
                    if (e->hasComponent<SpriteComponent>()) e->getComponent<SpriteComponent>().Play("fly");
                    break;
                }
                if (ai.divePhase == 2) {                                       // the dive
                    ai.diveT += lastFrameTime;
                    tr.velocity.x = ai.diveVX; tr.velocity.y = ai.diveVY;
                    tr.facingRight = ai.diveVX >= 0;
                    // bats ignore terrain collision, so stop at the ground
                    const int col = std::max(0, std::min(Level::WIDTH_TILES - 1,
                                     (int)((tr.position.x + 16.f) / Level::TILE)));
                    const int top = Level::stage().groundTop[col];
                    const bool atGround = top >= 0 && tr.position.y + tr.height * tr.scale >= (top + 1) * Level::TILE;
                    auto& ec = e->getComponent<ColliderComponent>().collider;
                    auto& pc = player.getComponent<ColliderComponent>().collider;
                    if (Collision::AABB(ec, pc)) {
                        if (Damage::applyToPlayer(player, Config::BAT_DIVE_DAMAGE, sink.shieldCharges,
                                                  sink.invincT, sink.iframeT, screenShake)) {
                            auto& ptr = player.getComponent<TransformComponent>();
                            ptr.velocity.x += (ai.diveVX >= 0 ? 1.f : -1.f) * 200.f;
                        }
                        ai.divePhase = 3;
                    }
                    if (atGround || ai.diveT >= Config::BAT_DIVE_TIME) ai.divePhase = 3;
                    if (e->hasComponent<SpriteComponent>()) e->getComponent<SpriteComponent>().Play("fly");
                    break;
                }
                if (ai.divePhase == 3) {                                       // climb back
                    tr.velocity.x *= 0.9f;
                    tr.velocity.y = -180.f;
                    if (tr.position.y <= ai.homeY) {
                        ai.divePhase = 0; ai.diveCd = Config::BAT_DIVE_CD;
                    }
                    if (e->hasComponent<SpriteComponent>()) e->getComponent<SpriteComponent>().Play("fly");
                    break;
                }
                if (dist < Config::ENEMY_CHASE_RANGE) {
                    float dirX = (pt.position.x > tr.position.x) ? 1.f : -1.f;
                    float dirY = (pt.position.y > tr.position.y) ? 1.f : -1.f;
                    tr.velocity.x = dirX * 160.f;
                    tr.velocity.y = dirY * 80.f + std::sin(ai.sinT * 4.f) * 60.f;
                } else {
                    if (std::abs(tr.position.x - ai.patrolCenter) > 180.f)
                        ai.patrolDir *= -1;
                    tr.velocity.x = ai.patrolDir * 90.f;
                    tr.velocity.y = std::sin(ai.sinT * Config::BAT_SIN_FREQ) * Config::BAT_SIN_AMP;
                }
                tr.facingRight = tr.velocity.x >= 0;
                if (e->hasComponent<SpriteComponent>())
                    e->getComponent<SpriteComponent>().Play("fly");
                break;
            }
            case EnemyAIComponent::Kind::Mage: {
                if (dist < Config::MAGE_FIRE_RANGE) {
                    ai.transition(EnemyAIComponent::State::Attack);
                    int dir = (pt.position.x > tr.position.x) ? 1 : -1;
                    tr.facingRight = dir > 0;
                    tr.velocity.x *= 0.8f;
                    if (ai.fireTimer <= 0) {
                        Vector2D v((float)(dir * Config::MAGE_PROJECTILE_SPEED), 0);
                        Combat::spawnProjectile(*e, v, Config::ENEMY_DAMAGE * 0.6f,
                                                "enemy", "fireball", 2.4f);
                        ai.fireTimer = Config::MAGE_FIRE_COOLDOWN;
                    }
                } else {
                    ai.transition(EnemyAIComponent::State::Chase);
                    float dir = (dx > 0) ? -1.f : 1.f;
                    tr.velocity.x = dir * 80.f;
                    tr.facingRight = dir > 0;
                }
                if (e->hasComponent<SpriteComponent>())
                    e->getComponent<SpriteComponent>().Play("idle");
                break;
            }

            case EnemyAIComponent::Kind::Archer: {
                int dir = (pt.position.x > tr.position.x) ? 1 : -1;
                tr.facingRight = dir > 0;

                if (adx > Config::ARCHER_RANGE) {
                    // out of range — hold position and scan
                    ai.transition(EnemyAIComponent::State::Patrol);
                    ai.aiming = false; ai.aimT = 0.f;
                    if (std::abs(tr.position.x - ai.patrolCenter) > Config::ENEMY_PATROL_R)
                        ai.patrolDir = (tr.position.x > ai.patrolCenter) ? -1 : 1;
                    tr.velocity.x = ai.patrolDir * 50.f;
                } else if (adx < Config::ARCHER_MIN_RANGE) {
                    // too close — back off while keeping the bow trained
                    ai.transition(EnemyAIComponent::State::Chase);
                    ai.aiming = false; ai.aimT = 0.f;
                    tr.velocity.x = -dir * Config::ARCHER_RETREAT_SPEED;
                } else {
                    // in the sweet spot — telegraph, then loose an arrow
                    ai.transition(EnemyAIComponent::State::Attack);
                    tr.velocity.x *= 0.75f;
                    if (!ai.aiming) {
                        ai.aiming = true;
                        ai.aimT = 0.f;
                    }
                    ai.aimT += lastFrameTime;
                    if (ai.aimT >= Config::ARCHER_AIM_TIME && ai.fireTimer <= 0) {
                        ai.aimT = 0.f;
                        ai.aiming = false;
                        Vector2D v((float)(dir * Config::ARCHER_ARROW_SPEED), 0);
                        Combat::spawnProjectile(*e, v, Config::ENEMY_DAMAGE * 0.75f,
                                                "enemy", "fireball", 2.4f);
                        ai.fireTimer = Config::ARCHER_FIRE_COOLDOWN;
                    }
                }
                if (e->hasComponent<SpriteComponent>()) {
                    e->getComponent<SpriteComponent>().Play(
                        ai.aiming ? "shoot" : "idle");
                }
                break;
            }

            case EnemyAIComponent::Kind::Bomber: {
                int dir = (pt.position.x > tr.position.x) ? 1 : -1;
                tr.facingRight = dir > 0;

                if (ai.fuseT < 0.f) {
                    // dormant — shamble about until the hero comes close
                    ai.transition(EnemyAIComponent::State::Patrol);
                    if (dist < Config::BOMBER_TRIGGER_RANGE) {
                        ai.fuseT = Config::BOMBER_FUSE_TIME;   // light it
                        ai.transition(EnemyAIComponent::State::Attack);
                        if (auto* s = Game::assets->GetSound("powerup"))
                            Mix_PlayChannel(-1, s, 0);
                    } else {
                        if (std::abs(tr.position.x - ai.patrolCenter) > Config::ENEMY_PATROL_R)
                            ai.patrolDir = (tr.position.x > ai.patrolCenter) ? -1 : 1;
                        tr.velocity.x = ai.patrolDir * 55.f;
                    }
                } else {
                    // fuse lit — commit fully to the charge, no turning back
                    ai.transition(EnemyAIComponent::State::Attack);
                    tr.velocity.x = dir * Config::BOMBER_SPEED;
                    ai.fuseT -= lastFrameTime;
                    if (ai.fuseT <= 0.f) {
                        detonate(*e, player, sink, screenShake);
                        continue;   // entity is gone; skip the rest
                    }
                }
                if (e->hasComponent<SpriteComponent>()) {
                    e->getComponent<SpriteComponent>().Play(
                        ai.fuseT >= 0.f ? "fuse" : "walk");
                }
                break;
            }

            case EnemyAIComponent::Kind::Boss: {
                bool enrage = hp.hp < hp.hpMax * Config::BOSS_ENRAGE_RATIO;
                ai.enraged = enrage;
                float speed = enrage ? Config::BOSS_SPEED * 1.3f : Config::BOSS_SPEED;
                bool slamming = (ai.slamPhase != 0);

                // ---- jump slam (phase 2) ----
                if (enrage) {
                    ai.slamT += lastFrameTime;
                    switch (ai.slamPhase) {
                        case 0:   // waiting for the cooldown
                            if (ai.slamT > Config::BOSS_SLAM_INTERVAL && adx < 420.f) {
                                ai.slamPhase = 1; ai.slamT = 0.f;
                            }
                            break;
                        case 1:   // crouch — the tell
                            tr.velocity.x *= 0.7f;
                            if (ai.slamT > Config::BOSS_SLAM_CHARGE) {
                                ai.slamPhase = 2; ai.slamT = 0.f;
                                tr.velocity.y = Config::BOSS_SLAM_RISE;
                                tr.velocity.x = (dx > 0 ? -1.f : 1.f) * 190.f;
                            }
                            break;
                        case 2:   // airborne; resolve once it comes back down
                            if (ai.slamT > 0.25f && tr.velocity.y >= 0.f) {
                                slamLanding(*e, screenShake);
                                ai.slamPhase = 3; ai.slamT = 0.f;
                            }
                            break;
                        case 3:   // recover
                            tr.velocity.x *= 0.6f;
                            if (ai.slamT > 0.55f) { ai.slamPhase = 0; ai.slamT = 0.f; }
                            break;
                        default: ai.slamPhase = 0; break;
                    }
                }

                // ---- ground movement / charge ----
                if (!slamming) {
                    if (adx < 600.f) {
                        ai.transition(EnemyAIComponent::State::Chase);
                        int dir = (dx > 0) ? -1 : 1;
                        tr.velocity.x = dir * speed;
                        tr.facingRight = dir > 0;
                        ai.chargeT += lastFrameTime;
                        if (ai.chargeT > Config::BOSS_CHARGE_TIME && adx < 240.f) {
                            tr.velocity.x = dir * Config::BOSS_CHARGE_SPEED;
                            ai.chargeT = 0;
                        }
                    } else {
                        ai.transition(EnemyAIComponent::State::Patrol);
                        if (std::abs(tr.position.x - ai.patrolCenter) > 200.f)
                            ai.patrolDir = (tr.position.x > ai.patrolCenter) ? -1 : 1;
                        tr.velocity.x = ai.patrolDir * 60.f;
                        tr.facingRight = ai.patrolDir > 0;
                    }
                }

                // ---- ranged ----
                ai.fireballT += lastFrameTime;
                if (!slamming) {
                    float interval = enrage ? Config::BOSS_PHASE2_INTERVAL
                                            : Config::BOSS_PHASE2_INTERVAL * 1.6f;
                    if (ai.fireballT > interval) {
                        ai.fireballT = 0;
                        int dir = (dx > 0) ? -1 : 1;
                        Vector2D v((float)(dir * Config::MAGE_PROJECTILE_SPEED * 0.9f), 0);
                        Combat::spawnProjectile(*e, v, Config::BOSS_DAMAGE * 0.5f,
                                                "enemy", "fireball", 2.4f);
                    }
                }
                // phase 2 extra: a spread of three bolts you have to weave through
                if (enrage && !slamming) {
                    ai.fanT += lastFrameTime;
                    if (ai.fanT > Config::BOSS_FAN_INTERVAL) {
                        ai.fanT = 0;
                        Combat::spawnProjectileFan(*e, player, Config::BOSS_FAN_COUNT,
                                                   Config::BOSS_FAN_SPREAD,
                                                   Config::MAGE_PROJECTILE_SPEED * 0.85f,
                                                   Config::BOSS_DAMAGE * 0.45f);
                    }
                }

                if (e->hasComponent<SpriteComponent>()) {
                    auto& sp = e->getComponent<SpriteComponent>();
                    if (ai.slamPhase == 1)     sp.Play("run");
                    else if (ai.slamPhase >= 2) sp.Play("attack");
                    else sp.Play(std::abs(tr.velocity.x) > 200.f ? "attack" : "run");
                }
                break;
            }
        }

        // damage-on-touch for ground enemies and the boss.
        // Bombers never deal contact damage — they only hurt when they blow up.
        if (ai.kind == EnemyAIComponent::Kind::Skeleton ||
            ai.kind == EnemyAIComponent::Kind::Slime ||
            ai.kind == EnemyAIComponent::Kind::Mage ||
            ai.kind == EnemyAIComponent::Kind::Archer ||
            ai.kind == EnemyAIComponent::Kind::Spitter ||
            ai.kind == EnemyAIComponent::Kind::Boss) {
            if (e->hasComponent<ColliderComponent>() &&
                player.hasComponent<ColliderComponent>()) {
                auto& ec = e->getComponent<ColliderComponent>().collider;
                auto& pc = player.getComponent<ColliderComponent>().collider;
                if (Collision::AABB(ec, pc)) {
                    float dmg = (ai.kind == EnemyAIComponent::Kind::Boss)
                               ? Config::BOSS_DAMAGE : Config::ENEMY_DAMAGE;
                    if (Damage::applyToPlayer(player, dmg, sink.shieldCharges,
                                              sink.invincT, sink.iframeT,
                                              screenShake)) {
                        auto& ptr = player.getComponent<TransformComponent>();
                        int dir = (ptr.position.x < tr.position.x) ? 1 : -1;
                        ptr.velocity.x += dir * 220.f;
                        ptr.velocity.y = -260.f;
                    }
                }
            }
        }
    }
}

bool blocks(Entity& enemy, float fromX) {
    if (!enemy.hasComponent<EnemyAIComponent>()) return false;
    auto& ai = enemy.getComponent<EnemyAIComponent>();
    if (!ai.guarding || ai.hurtT > 0.f) return false;
    auto& tr = enemy.getComponent<TransformComponent>();
    const float cx = tr.position.x + tr.width * tr.scale * 0.5f;
    return tr.facingRight ? fromX >= cx : fromX <= cx;
}

} // namespace EnemyAI

// ===========================================================================
// Pickups
// ===========================================================================
namespace Pickup {

static bool overlaps(Entity& a, Entity& b) {
    if (!a.hasComponent<ColliderComponent>() || !b.hasComponent<ColliderComponent>()) return false;
    return Collision::AABB(a.getComponent<ColliderComponent>().collider,
                           b.getComponent<ColliderComponent>().collider);
}

// A solid thing can only ever be touched, never overlapped: test against its
// collider grown by `pad` px on every side.
static bool touches(Entity& a, Entity& b, int pad) {
    if (!a.hasComponent<ColliderComponent>() || !b.hasComponent<ColliderComponent>()) return false;
    SDL_Rect r = b.getComponent<ColliderComponent>().collider;
    r.x -= pad; r.y -= pad; r.w += 2 * pad; r.h += 2 * pad;
    return Collision::AABB(a.getComponent<ColliderComponent>().collider, r);
}

void step(Entity& player,
          std::vector<Entity*>& coins,
          std::vector<Entity*>& powerups,
          std::vector<Entity*>& facilities,
          std::vector<Entity*>& hazards,
          PlayerBag bag) {
    auto& ptr = player.getComponent<TransformComponent>();
    float pcx = ptr.position.x + ptr.width  / 2.f;
    float pcy = ptr.position.y + ptr.height / 2.f;

    // coins — while the magnet is live they home in on the hero instead of
    // waiting to be walked over.
    for (auto* c : coins) {
        if (!c->isActive()) continue;
        auto& ct = c->getComponent<TransformComponent>();
        if (bag.magnetT > 0.f) {
            float cx = ct.position.x + ct.width  / 2.f;
            float cy = ct.position.y + ct.height / 2.f;
            float dx = pcx - cx, dy = pcy - cy;
            float d2 = dx*dx + dy*dy;
            if (d2 < Config::POWERUP_MAGNET_RADIUS * Config::POWERUP_MAGNET_RADIUS
                && d2 > 1.f) {
                float d = std::sqrt(d2);
                // pull harder the closer it gets, so it snaps satisfyingly
                float pull = Config::POWERUP_MAGNET_PULL * (0.35f + 0.65f * (1.f - d / Config::POWERUP_MAGNET_RADIUS));
                ct.position.x += (dx / d) * pull * lastFrameTime;
                ct.position.y += (dy / d) * pull * lastFrameTime;
                if (c->hasComponent<ColliderComponent>())
                    c->getComponent<ColliderComponent>().update();
            }
        }
        if (overlaps(player, *c)) {
            int worth = Config::COIN_SCORE * (bag.coinMultT > 0.f ? Config::POWERUP_COIN_MULT : 1);
            bag.score += worth;
            bag.coinsBanked += 1;
            c->destroy();
            if (auto* s = Game::assets->GetSound("coin")) Mix_PlayChannel(-1, s, 0);
            Particles::emitDust(pcx, pcy, 255, 230, 90);
        }
    }
    // powerups
    for (auto* p : powerups) {
        if (!p->isActive()) continue;
        if (overlaps(player, *p)) {
            auto& pu = p->getComponent<PowerUpComponent>();
            float px = p->getComponent<TransformComponent>().position.x + 16;
            float py = p->getComponent<TransformComponent>().position.y + 16;
            switch (pu.type) {
                case PowerUpType::Health:
                    bag.hp = std::min(bag.maxHp, bag.hp + 35.f);
                    if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
                    Particles::emitHeal(px, py);
                    break;
                case PowerUpType::Speed:
                    bag.speedT = Config::POWERUP_DURATION[1];
                    if (auto* s = Game::assets->GetSound("speedBoost")) Mix_PlayChannel(-1, s, 0);
                    break;
                case PowerUpType::Invincible:
                    bag.invincT = Config::POWERUP_DURATION[2];
                    if (auto* s = Game::assets->GetSound("invincible")) Mix_PlayChannel(-1, s, 0);
                    break;
                case PowerUpType::DoubleJump:
                    bag.jumpsLeft = bag.maxJumps;
                    if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
                    break;
                case PowerUpType::Damage:
                    bag.dmgT = 8.f;
                    if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
                    break;
                case PowerUpType::Magnet:
                    bag.magnetT = Config::POWERUP_DURATION[5];
                    if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
                    Particles::emitDust(px, py, 120, 190, 255);
                    break;
                case PowerUpType::Shield:
                    bag.shieldCharges = std::min(bag.shieldCharges + 1, 3);
                    if (auto* s = Game::assets->GetSound("invincible")) Mix_PlayChannel(-1, s, 0);
                    Particles::emitDust(px, py, 150, 220, 255);
                    break;
                case PowerUpType::CoinX2:
                    bag.coinMultT = Config::POWERUP_DURATION[7];
                    if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
                    Particles::emitDust(px, py, 255, 215, 90);
                    break;
            }
            bag.score += Config::POWERUP_SCORE;
            p->destroy();
        }
    }
    // facilities
    for (auto* f : facilities) {
        if (!f->isActive()) continue;
        const std::string& kind = f->tag;
        // the door is solid while locked (Game::addLockedDoors), so touch it
        if (!(kind == "door" ? touches(player, *f, 3) : overlaps(player, *f))) continue;
        if (kind == "checkpoint") {
            bag.reachedCheckpoint = true;
            // Batch B: the respawn point moves forward, never back.
            auto& ft = f->getComponent<TransformComponent>();
            const float cx = ft.position.x + ft.width * ft.scale * 0.5f - Config::PLAYER_W * 0.5f;
            if (cx > bag.respawnX + 1.f) {
                bag.respawnX = cx;
                bag.respawnY = ft.position.y + ft.height * ft.scale - Config::PLAYER_H - 1.f;
                if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
                Particles::emitHeal(ft.position.x + 16, ft.position.y + 16);
            }
        } else if (kind == "shrine") {
            bag.hp = bag.maxHp;
            float fx = f->getComponent<TransformComponent>().position.x + 16;
            float fy = f->getComponent<TransformComponent>().position.y + 16;
            if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
            Particles::emitHeal(fx, fy);
        } else if (kind == "key") {
            bag.keys++;
            bag.score += Config::KEY_SCORE;
            f->destroy();
            if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
        } else if (kind == "door") {
            if (bag.keys > 0 && !bag.doorUnlocked) {
                bag.doorUnlocked = true;
                bag.keys--;
                f->destroy();
                if (auto* s = Game::assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
            }
        }
    }
    // hazards
    if (bag.iframeT <= 0.f) {
        for (auto* h : hazards) {
            if (!h->isActive()) continue;
            if (overlaps(player, *h)) {
                if (Damage::applyToPlayer(player, Config::HAZARD_DMG_SPIKE,
                                          bag.shieldCharges, bag.invincT, bag.iframeT,
                                          bag.shake)) {
                    // knock the hero up off the spikes so they aren't
                    // re-hit on the very next frame
                    auto& tr = player.getComponent<TransformComponent>();
                    tr.velocity.y = std::min(tr.velocity.y, -320.f);
                }
            }
        }
    }
}

} // namespace Pickup

// ===========================================================================
// Camera
// ===========================================================================
namespace CameraSystem {

void follow(Entity& player, SDL_Rect& camera, float& shake, float& shakeMag) {
    auto& tr = player.getComponent<TransformComponent>();
    float px = tr.position.x + tr.width/2.f;
    float py = tr.position.y + tr.height/2.f;
    float targetX = px - Config::WINDOW_WIDTH/2.f;
    float targetY = py - Config::WINDOW_HEIGHT/2.f;
    float relX = (camera.x + Config::WINDOW_WIDTH/2.f) - px;
    float relY = (camera.y + Config::WINDOW_HEIGHT/2.f) - py;
    if (std::abs(relX) < Config::CAM_DEAD_ZONE_X) targetX = camera.x;
    if (std::abs(relY) < Config::CAM_DEAD_ZONE_Y) targetY = camera.y;
    targetX += tr.facingRight ? Config::CAM_LOOKAHEAD : -Config::CAM_LOOKAHEAD;

    camera.x = (int)(camera.x + (targetX - camera.x) * Config::CAM_LERP);
    camera.y = (int)(camera.y + (targetY - camera.y) * Config::CAM_LERP);

    int maxX = Config::MAP_PIXEL_WIDTH  - Config::WINDOW_WIDTH;
    int maxY = Config::MAP_PIXEL_HEIGHT - Config::WINDOW_HEIGHT;
    if (camera.x < 0) camera.x = 0;
    if (camera.y < 0) camera.y = 0;
    if (camera.x > maxX) camera.x = maxX;
    if (camera.y > maxY) camera.y = maxY;

    if (shake > 0.01f) {
        float sx = (std::sin(shake * 31.f) * 0.5f) * shakeMag;
        float sy = (std::cos(shake * 27.f) * 0.5f) * shakeMag;
        camera.x += (int)sx;
        camera.y += (int)sy;
    }
    shake    = std::max(0.f, shake    - Config::CAM_SHAKE_DECAY * lastFrameTime);
    shakeMag = std::max(0.f, shakeMag - Config::CAM_SHAKE_DECAY * lastFrameTime);
}

} // namespace CameraSystem
