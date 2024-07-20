#pragma once
#include <iostream>

class Vector2D {
public:
    float x;
    float y;

    Vector2D();
    Vector2D(float x, float y);

    Vector2D& Add(const Vector2D& vec);
    Vector2D& Subtract(const Vector2D& vec);
    Vector2D& Multiply(const Vector2D& vec);
    Vector2D& Divide(const Vector2D& vec);

    Vector2D operator+(const Vector2D& v2) const;
    Vector2D operator-(const Vector2D& v2) const;
    Vector2D operator*(const Vector2D& v2) const;
    Vector2D operator/(const Vector2D& v2) const;
    Vector2D operator*(const float& f) const;

    Vector2D& operator+=(const Vector2D& vec);
    Vector2D& operator-=(const Vector2D& vec);
    Vector2D& operator*=(const Vector2D& vec);
    Vector2D& operator/=(const Vector2D& vec);

    bool operator==(const Vector2D vec);
    bool operator!=(const Vector2D vec);

    Vector2D operator/(const int& i) const;
    Vector2D& Zero();

    friend std::ostream& operator<<(std::ostream& stream, const Vector2D& vec);
};

namespace sb {
struct Rect {
    Vector2D pos;
    Vector2D size;
};
}