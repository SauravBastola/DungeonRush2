#pragma once

#include <SDL.h>
#include "Vector2D.h"

class ColliderComponent;

class Collision {
public:
    static bool AABB(const SDL_Rect& recA, const SDL_Rect& recB);
    static bool AABB(const ColliderComponent& colA, const ColliderComponent& colB);

    // Advanced collision

    static bool PointVsRect(const Vector2D& point, const sb::Rect& rect);
    static bool RectVsRect(const sb::Rect& recA, const sb::Rect& recB);
    static bool RayVsRect(Vector2D rayOrigin, Vector2D rayDirection, const sb::Rect targetRect,
                          Vector2D& contactPoint, Vector2D& contactNormal, float& t_hit_near);
    static bool DynamicRectVsRect(const sb::Rect rect, Vector2D& rectVelocity, const sb::Rect targetRect,
                                  Vector2D& contactPoint, Vector2D& contactNormal, float& contactTime, float fElapsedTime);
};
