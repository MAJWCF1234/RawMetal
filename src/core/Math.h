#pragma once

#include <algorithm>
#include <cmath>

namespace retro {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
};

inline float dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
inline float lengthSq(const Vec2& v) { return dot(v, v); }
inline float length(const Vec2& v) { return std::sqrt(lengthSq(v)); }
inline Vec2 normalized(const Vec2& v) {
    const float len = length(v);
    return len > 0.00001f ? v * (1.0f / len) : Vec2{};
}
inline float clamp(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }
inline float wrapAngle(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}
}
