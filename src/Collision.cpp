#include "Collision.h"
#include "ColliderComponent.h"

extern float lastFrameTime;

bool Collision::AABB(const SDL_Rect& recA, const SDL_Rect& recB) {
    if (
        recA.x + recA.w >= recB.x &&
            recB.x + recB.w >= recA.x &&
            recA.y + recA.h >= recB.y &&
            recB.y + recB.h >= recA.y
        ) {
        return true;
    }

    return false;
}

bool Collision::AABB(const ColliderComponent& colA, const ColliderComponent& colB) {
    if (AABB(colA.collider, colB.collider)) {
        return true;
    }
    else {
        return false;
    }
}

bool Collision::PointVsRect(const Vector2D& point, const sb::Rect& rect) {
    if (point.x >= rect.pos.x && point.y >= rect.pos.y && point.x < rect.pos.x + rect.size.x && point.y < rect.pos.y + rect.size.y) {
        return true;
    }
    else {
        return false;
    }
}

bool Collision::RectVsRect(const sb::Rect& recA, const sb::Rect& recB) {
    if (recA.pos.x + recA.size.x > recB.pos.x &&
        recB.pos.x + recB.size.x > recA.pos.x &&
        recA.pos.y + recA.size.y > recB.pos.y &&
        recB.pos.y + recB.size.y > recA.pos.y) {
        return true;
    }
    else {
        return false;
    }
}

bool Collision::RayVsRect(Vector2D rayOrigin, Vector2D rayDirection, const sb::Rect targetRect,
                          Vector2D& contactPoint, Vector2D& contactNormal, float& t_hit_near) {

    contactNormal = {0, 0};
    contactPoint = {0, 0};

    Vector2D t_near = (targetRect.pos - rayOrigin) / rayDirection;
    Vector2D t_far = (targetRect.pos + targetRect.size - rayOrigin) / rayDirection;

    if (std::isnan(t_far.y) || std::isnan(t_far.x))
        return false;
    if (std::isnan(t_near.y) || std::isnan(t_near.x))
        return false;

    if (t_near.x > t_far.x)
        std::swap(t_near.x, t_far.x);
    if (t_near.y > t_far.y)
        std::swap(t_near.y, t_far.y);

    if (t_near.x > t_far.y || t_near.y > t_far.x)
        return false;

    t_hit_near = std::max(t_near.x, t_near.y);
    float t_hit_far = std::min(t_far.x, t_far.y);

    if (t_hit_far < 0)
        return false;

    contactPoint = rayOrigin + rayDirection * t_hit_near;

    if (t_near.x > t_near.y) {
        if (rayDirection.x < 0) {
            contactNormal = {1, 0};
        }
        else {
            contactNormal = {-1, 0};
        }
    }
    else if (t_near.x < t_near.y) {
        if (rayDirection.y < 0) {
            contactNormal = {0, 1};
        }
        else {
            contactNormal = {0, -1};
        }
    }

    return true;
}
bool Collision::DynamicRectVsRect(const sb::Rect rect, Vector2D& rectVelocity, const sb::Rect targetRect,
                                  Vector2D& contactPoint, Vector2D& contactNormal, float& contactTime, float fElapsedTime) {


    if (rectVelocity.x == 0 && rectVelocity.y == 0)
        return false;

    sb::Rect expanded_target;

    expanded_target.pos = targetRect.pos - rect.size / 2;
    expanded_target.size = targetRect.size + rect.size;

    if (RayVsRect(rect.pos + rect.size / 2, rectVelocity * fElapsedTime, expanded_target, contactPoint, contactNormal, contactTime)) {
        return (contactTime >= 0.0f && contactTime <= 1.0f);
    }
    return false;
}
