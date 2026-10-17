# Opus task: DungeonRush2 architecture, design, and staged-upgrade plan

You are acting as the lead software architect, gameplay systems designer, technical director, and reviewer for an existing C++ 2D platformer—not creating a game from scratch.

I am uploading a ZIP of the existing repository, `DungeonRush2-opus-handoff-*.zip`. Read the source archive before making recommendations. Treat source files and generators as evidence; do not assume the README is perfectly current. I need a coherent plan and a small documentation handoff that can later be executed by a different coding agent (DeepSeek) in a bounded, testable way. I can bring your output back into a local coding workflow, but I do not have your model available through an API or Claude Code.

The archive is not source-only. It also contains a completed Phase 0 baseline audit — `BASELINE_REPORT.md`, `screenshots/`, and `logs/` — plus the operating agreement in `DungeonRush2_DEEPSEEK_ITERATIVE_WORKFLOW.md`. **Read `BASELINE_REPORT.md` first.** It records what was actually built, run and captured, so you can spend your reasoning on architecture and design instead of re-deriving baseline facts. Where the report and the README disagree, prefer the report; where the report and the source disagree, prefer the source and say so.

## My goals

I want to substantially improve and expand DungeonRush2 while preserving the working game and avoiding a reckless rewrite.

- Separate a reusable 2D engine from the DungeonRush2 game built on it.
- Use a clear entity-component-system style where appropriate, with explicit ownership and clean engine/game dependency direction.
- Improve code organization, testability, maintainability, and consistency of implementation.
- Improve environment art and make sprites, enemies, tiles, VFX, HUD, and menus feel like one coherent game.
- Support fullscreen and windowed modes reliably, with sensible scaling and UI layout at different window sizes/resolutions.
- Add more varied and intelligent enemy behavior, objectives, portals, progression, and other mechanics that strengthen the core game rather than becoming disconnected feature bloat.
- Improve menus/settings and player-facing feedback without needlessly discarding a menu system that already works.
- Keep the asset and level-generation workflow reproducible.

I am open to Rust or Python for a specific supporting tool if there is a concrete reason, but **do not propose porting the game away from C++ as a default**. Evaluate migration only if you can demonstrate a meaningful benefit that outweighs rewriting existing systems and the cost of losing momentum. Retaining C++17 and SDL2 is the default assumption for the first roadmap.

## Verified baseline (already measured — use as evidence, do not re-litigate)

A local agent completed a Phase 0 audit on macOS 27.0.1 / arm64 / Apple M3 Pro. Details and raw logs are in `BASELINE_REPORT.md`, `screenshots/` and `logs/`.

- **The build is not broken.** The documented `./build.sh` fails on that host inside CMake's *compiler sanity check*, because the CommandLineTools macOS 27 SDK declares an architecture slice (`arm64e.x1-macos`) that Xcode's `ld-1266.8` rejects. It is a **host toolchain mismatch, not a source or CMake defect**. With `SDKROOT=<Xcode MacOSX26.4.sdk>` the documented script passes and produces **zero compiler warnings** under `-Wall -Wextra -Wpedantic`, and a byte-identical binary to the `-DCMAKE_OSX_SYSROOT` build. Do not conclude that the project does not compile, and do not propose source changes to fix it.
- **The game runs.** It launches from the project root and is playable: movement, jumping, terrain collision, coin pickup, melee, dash, damage and the pause menu were exercised successfully.
- **Screenshot provenance matters.** The images in `screenshots/` were captured *headlessly*: real game object files, dummy video driver, software renderer, `SDL_RenderReadPixels`. They show exactly what the renderer draws, but they are **not** desktop screenshots and cannot prove window management, fullscreen, resize, audio or frame pacing. OS-level capture is blocked on that machine by macOS TCC permissions. Judge art, layout and HUD from them; do not treat them as evidence about the windowing layer.
- **Confirmed defects** (each with code evidence in the report): (a) starting a run consumes one life — `State::Menu`/`Instructions` run `manager.update()` without `stepPlayerPhysics()`, so gravity integrates with no collision and the hero falls out of the world while the menu is up; (b) the boss bar is drawn for any live boss regardless of distance, so it is visible from the level start; (c) every text draw uses `TTF_RenderText_Blended` (Latin-1), so `·` renders as `Â·`; (d) the How To Play header overlaps its own subtitle; (e) the sun and clouds are hard-edged `SDL_RenderFillRect` stacks by implementation, not a failed asset load; (f) there is no fullscreen path and no resize path anywhere in `src/`.
- **Not verified:** audio, frame pacing, the boss fight, key/door progression, checkpoint activation, victory and game-over transitions, and whether the level is completable.

## Initial facts to verify against the archive

These are preliminary observations, not unquestionable requirements. Verify them from the uploaded files and correct them if necessary.

- The project builds as a C++17 CMake target using SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer through `pkg-config`.
- The README describes the game as “DungeonRush2 — The Sunken Path.” It describes a generated tile-based world, movement features, combat, several enemies, powerups, checkpoints, hazards, moving platforms, a boss, UI, and procedural atmosphere.
- The project has its own `Component`, `Entity`, and `Manager` implementation in `src/ECS/ECS.h`; it has a bitset/component-array design and components are owned by entities. The README refers to an “Apollo” ECS core, which may be stale wording or a project codename; verify rather than assuming an external ECS dependency exists.
- `src/Game.cpp` is large and handles many jobs: setup/loading, spawning, input, gameplay state, update orchestration, combat glue, settings persistence, HUD, menus, instructions, world rendering, and state overlays. `src/Game.h` exposes many game state fields.
- `src/Systems/Systems.cpp` holds several functional gameplay systems in namespaces (physics, combat, AI, pickups, damage, particles, camera), while `Parallax`, `Atmosphere`, `Scenery`, and `Vfx` are separate modules.
- There is substantial global/static state, including a global manager/map and frame-time variables. Several components use global timing variables.
- `tools/build_level.py` generates `src/Level.h` and several map/collider files; `tools/build_objects.py` generates `src/Objects.h` and an object atlas. `src/SpriteSpec.h` centralizes sprite sizing/anchors/collider relationships. Do not hand-edit generated outputs without updating their generators.
- The README describes the main loop as fixed-step, but `src/main.cpp` appears to use a variable delta that is clamped. Verify this mismatch and propose an appropriate physics/update timing model.
- Fullscreen is an argument to `Game::init`, but `main.cpp` passes `false`. Camera and UI code appear to use configured window dimensions in multiple places. Design fullscreen/resizing as a complete rendering/viewport feature, not merely a startup flag.
- No test targets are obvious in the supplied `CMakeLists.txt`; verify whether tests exist elsewhere in the ZIP.
- The assets show several different visual treatments. Inspect representative images and sprite sheets in the archive before recommending an art direction.

## Hard constraints

1. **Do not perform a wholesale rewrite in this first pass.** I want analysis, decisions, interfaces, migration sequencing, and acceptance criteria first. Do not silently modify runtime source or create a large replacement implementation.
2. Preserve working mechanics unless a specific defect or design reason is documented. Distinguish a verified defect from a suspected risk or a future design choice.
3. Do not invent test results, build status, gameplay screenshots, profiling data, or behavior you have not verified. State clearly what could not be established from the archive alone.
4. Make plans incremental. Each phase must leave the repository buildable and the game in a demonstrably usable state. Propose a rollback/checkpoint strategy.
5. Prefer the smallest abstraction that solves a real problem. Do not fragment the code into many libraries/classes merely to make the diagram look “enterprise.”
6. Keep generated artifacts and their generators in sync. Treat generated `Level.h` as output, not the authoring source. Preserve the current tile-unit/scale/collider conventions unless a deliberate migration is justified.
7. Inspect update order and entity/component lifecycle carefully. The README documents a physics ownership invariant: physics-driven bodies move through `Physics::moveAndCollide`, while `TransformComponent::update()` may integrate other entities. The exact movement, collision, platform-carry, timer, refresh, and render order may be gameplay-critical. Do not break or casually reorder it.
8. Do not add new major third-party dependencies without comparing cost, platform support, and maintenance burden. Keep macOS as a first-class development environment; Linux should remain feasible if supported by the project.
9. For graphics/resolution, preserve aspect ratio and pixel/art readability as appropriate, avoid stretching sprites, and ensure menus/HUD remain legible. Explicitly compare a fixed logical resolution with viewport scaling versus a genuinely resizable virtual resolution; recommend one and explain tradeoffs.
10. Make no license claim beyond the evidence. The README currently has a placeholder-like license statement; flag it for owner confirmation rather than guessing.
11. Keep secrets out of all output and ask for confirmation before suggesting external asset downloads or new licensed content.
12. Do not write an enormous one-shot implementation task for the weaker implementation agent. Break work into short, bounded tasks with explicit files, behavior, tests, and acceptance criteria.

## What I want you to produce in this first pass

Inspect the repository and produce a **planning packet**, preferably as separate downloadable Markdown files in a `docs/architecture-plan/` folder (or equivalent attachments). If your interface cannot provide files, return complete, clearly separated Markdown documents in a concise output order. Avoid dumping all source code back to me.

### 1. `PROJECT_AUDIT.md`

Provide:
- Current module/file map and responsibilities based on actual code.
- Current build/dependency pipeline and content-generation pipeline.
- Actual vs claimed README behavior, including inconsistencies.
- Key architecture risks ranked by severity and confidence.
- Likely current invariants and fragile couplings that must be protected.
- What needs a real baseline build/run or screenshot to verify.

Use evidence such as file paths and function/type names. Do not invent line numbers if you have not checked them.

### 2. `GAME_VISION_AND_PILLARS.md`

Develop a coherent direction for DungeonRush2 consistent with its existing title/theme and action-platformer identity. Define:
- 3–5 design pillars and a clear player fantasy.
- Core gameplay loop, short-term and long-term progression.
- What makes each enemy archetype tactically distinct; “smarter” AI should mean readable, fair, testable behavior rather than simply more health or more damage.
- Objective/portal/checkpoint/level-completion structure and how goals communicate to the player.
- A prioritization method that distinguishes foundational needs from optional features.

Do not assume every requested feature must be added. Explain which features support the pillars and which should be deferred.

### 3. `ART_DIRECTION.md`

Actually inspect representative sprite sheets, terrain, UI, generated artwork and effects in the archive. Produce a style guide that gives concrete rules for:
- Palette and semantic color use; outline weight and shading language.
- Sprite silhouette, proportions, rendering size, animation frame cadence, and anchor conventions.
- Environment layering, terrain repetition, background/parallax depth, lighting/atmosphere, and negative space.
- Enemy readability and telegraphing; projectile, attack, hit, death, pickup, portal, and boss feedback.
- HUD/menu typography, spacing, states, focus, transitions, accessibility/readability, and fullscreen scaling.
- Asset naming, source/export formats, resolution, and how PNG/SVG generation stays synchronized.

First diagnose what should be preserved, unified, redrawn, or deprecated. Avoid the vague instruction “make it beautiful.” Do not redraw assets yet. If you recommend a substantially different style, explain migration cost and give a sample implementation specification for one test asset before a whole-art pass.

### 4. `TARGET_ARCHITECTURE.md`

Propose a practical engine/game boundary. Include:
- Responsibilities belonging to the reusable engine vs DungeonRush2.
- Recommended top-level project structure and CMake target/module plan.
- Dependency direction (preferably engine does not include game-specific headers or know DungeonRush2 enemy/objective rules).
- Runtime lifecycle and ownership for SDL window/renderer, timing, assets/resources, audio, input, ECS/world, scene/level, camera, UI, collision, save/settings, and logging.
- An ECS recommendation grounded in current code: retain and harden the custom ECS, or replace it only if a requirements-based comparison justifies doing so. Define component/system contracts, entity lifetime, safe mutation while iterating, system scheduling, and component validation.
- Data flow for level definitions/generated content, runtime entities, resources and UI.
- Fullscreen, window resize, logical resolution, letterboxing/viewport, DPI considerations, and coordinate conversion.
- Timing recommendation (fixed-step accumulator, variable-step, or hybrid), including how render timing and UI/effects should behave.
- Error handling/resource cleanup and configuration/save paths.
- What not to abstract yet.

Avoid proposing an engine architecture too broad for a single small 2D platformer. Compare one executable with logical modules versus separate engine/game libraries and recommend a staged option.

### 5. `MIGRATION_ROADMAP.md`

Break work into phases that can be executed and verified independently. For every phase specify:
- Goal and value to the player/developer.
- Scope and files/modules expected to change.
- Dependencies and risks.
- Tests/manual validation and acceptance criteria.
- Exit criteria and rollback/checkpoint.
- Suggested owner: Opus for difficult design/architectural changes, DeepSeek for bounded implementation, or human review where necessary.

Include baseline capture/build, test/generation quality gates, application/window/render abstraction, engine/game separation, ECS/lifecycle/time hardening, visual consistency, and new gameplay vertical slices. Choose sequence based on risk reduction and ability to verify. Do not put every new feature in Phase 1.

### 6. `DEEPSEEK_AGENT_PROTOCOL.md`

Write reusable operating rules for the implementation agent:
- One bounded task per turn/branch.
- Read relevant code and architecture docs first; identify exact files and assumptions before editing.
- No unrelated formatting/rewrite; preserve behavior outside acceptance criteria.
- Run the required build/tests/generators after changes and report actual command output/results.
- Avoid replacing generated source without changing the generator.
- Use small diffs, explain files changed, risks, test evidence, manual test steps, and rollback.
- Stop and ask if a task requires an architectural decision or conflicts with a written invariant.
- No claim that something works unless the test/build was actually run.

### 7. `FIRST_IMPLEMENTATION_BATCH.md`

Recommend the smallest useful first batch after the planning pass. It should improve project reliability or create a high-value foundation, not attempt the entire architecture. List exact tasks in dependency order, acceptance checks, and what must be deferred.

The reproducible baseline already exists, so do not spend batch 1 on obtaining one. Instead, **triage the confirmed defects** listed above and state explicitly which ones belong in batch 1, which are deferred and why, and which are design decisions rather than defects. For each defect you include, name the file and function you expect to change, the invariant at risk (especially the physics-ownership and update-order rules in the README), and the observable acceptance check — including how a headless capture could show before/after. The menu-idle life loss is the obvious candidate for a first slice, but justify or reject it rather than assuming it.

### 8. Decision log

Include a brief `DECISIONS.md` or an equivalent section with:
- Decisions you recommend now and why.
- Decisions intentionally deferred until after baseline testing.
- Open questions that genuinely require my input.
- Options considered and rejected, including reasons not to port languages by default.

## Review approach

Work in this order:

1. Inventory and inspect the project, including core source and relevant generators.
2. Verify the README against actual code; note uncertainty.
3. Identify critical invariants and failure-prone couplings.
4. Propose design/architecture options with tradeoffs and one recommended default.
5. Create the planning packet.
6. Give me a short executive summary and a prioritized “first five actions” list.

Do not ask me a long questionnaire before doing the inspection. Where preferences are genuinely unknown, state a reasonable assumption, give alternatives briefly, and mark the choice for review.

## Expected model handoff

Explicitly divide work among:
- **Opus:** deep architectural reasoning, technical design, tricky foundational code or refactors only after the plan is approved, and review of high-risk changes.
- **DeepSeek:** well-scoped implementation tasks with specific files, exact behavior, acceptance criteria, and build/test commands. It can build with `SDKROOT` set, run the game headlessly, drive scripted input, and capture render output to PNG for before/after comparison — so it can verify rendering and logic without a display. It cannot take desktop screenshots or interact with a real window on that machine.
- **Human (me):** approve major design decisions, maintain Git checkpoints, observe actual on-screen gameplay where the agent cannot (window behaviour, fullscreen, audio, feel), and supply screenshots or error logs when the agent is blocked by OS permissions.

Write the first implementation task so that DeepSeek can verify it end-to-end without a human in the loop where possible, and state exactly what only a human can confirm.

End with the recommended first implementation task as a self-contained specification I can later give to DeepSeek. Do not begin implementing it now.
