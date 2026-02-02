#pragma once

#include <algorithm>

namespace STRN
{

struct Vec2
{
    int x = 0;
    int y = 0;
    
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(const Vec2& other) { x *= other.x; y *= other.y; return *this; }
    Vec2& operator/=(const Vec2& other) { x /= other.x; y /= other.y; return *this; }
    Vec2& operator*=(const int other) { x *= other; y *= other; return *this; }
    Vec2& operator/=(const int other) { x /= other; y /= other; return *this; }
    bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
};

inline Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{ a.x + b.x, a.y + b.y }; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{ a.x - b.x, a.y - b.y }; }
inline Vec2 operator*(const Vec2& a, const Vec2& b) { return Vec2{ a.x * b.x, a.y * b.y }; }
inline Vec2 operator/(const Vec2& a, const Vec2& b) { return Vec2{ a.x / b.x, a.y / b.y }; }
inline Vec2 operator*(const Vec2& a, const int b) { return Vec2{ a.x * b, a.y * b }; }
inline Vec2 operator/(const Vec2& a, const int b) { return Vec2{ a.x / b, a.y / b }; }
inline Vec2 maxi(const Vec2& a, const int b) { return Vec2{ std::max(a.x, b), std::max(a.y, b) }; }
inline Vec2 mini(const Vec2& a, const Vec2& b) { return Vec2{ std::min(a.x, b.x), std::min(a.y, b.y) }; }
inline Vec2 maxi(const Vec2& a, const Vec2& b) { return Vec2{ std::max(a.x, b.x), std::max(a.y, b.y) }; }

struct Box2
{
    Vec2 min;
    Vec2 max;
    
    Vec2 size() const { return max - min; }
};

struct Transform2
{
    Vec2 position;
    Vec2 size;
    int z = 0;
    
    int left_clip = 0;
    int right_clip = 0;
    int top_clip = 0;
    int bottom_clip = 0;
};

Vec2 clip(Vec2 min_size, Vec2 max_size, Vec2 available_size);

}