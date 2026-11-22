// core.h — shared math, types and small utilities for NEON ARENA
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

#define PI 3.14159265358979f
#define TAU 6.28318530717959f
#define DEG2RAD(x) ((x) * PI / 180.0f)

// ---------------------------------------------------------------- Vec2
struct Vec2 {
    float x = 0, y = 0;
    Vec2() {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator*(Vec2 o) const { return {x * o.x, y * o.y}; }
};

// ---------------------------------------------------------------- Vec3
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(Vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};
inline Vec3 operator*(float s, Vec3 v) { return v * s; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(Vec3 v) { return sqrtf(dot(v, v)); }
inline float len2(Vec3 v) { return dot(v, v); }
inline float len(Vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }
inline Vec3 norm(Vec3 v) { float l = len(v); return l > 1e-8f ? v * (1.0f / l) : Vec3(0, 0, 0); }
inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline Vec3 rotateY(Vec3 v, float a) {
    float c = cosf(a), s = sinf(a);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

// Shortest-arc angle lerp (radians).
inline float lerpAngle(float a, float b, float t) {
    float d = b - a;
    while (d > PI) d -= TAU;
    while (d < -PI) d += TAU;
    return a + d * t;
}

// ---------------------------------------------------------------- Vec4 (color-friendly)
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 1;
    Vec4() {}
    Vec4(float x_, float y_, float z_, float w_ = 1) : x(x_), y(y_), z(z_), w(w_) {}
};

// ---------------------------------------------------------------- Mat4 (column-major, matches OpenGL)
struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};
};
inline Mat4 matMul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int rw = 0; rw < 4; rw++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + rw] * b.m[c * 4 + k];
            r.m[c * 4 + rw] = s;
        }
    return r;
}
inline Vec3 mulPoint(const Mat4& m, Vec3 v) {
    return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z,
            m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
            m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z};
}
inline Mat4 matTranslate(Vec3 t) {
    Mat4 r;
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
inline Mat4 matScale(Vec3 s) {
    Mat4 r;
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
inline Mat4 matScale(float s) { return matScale({s, s, s}); }
inline Mat4 matRotateX(float a) {
    Mat4 r; float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
    return r;
}
inline Mat4 matRotateY(float a) {
    Mat4 r; float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
    return r;
}
inline Mat4 matRotateZ(float a) {
    Mat4 r; float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
    return r;
}
inline Mat4 matTRS(Vec3 t, float yaw, float pitch, float roll, Vec3 s) {
    return matMul(matTranslate(t),
           matMul(matMul(matRotateY(yaw), matMul(matRotateX(pitch), matRotateZ(roll))), matScale(s)));
}
inline Mat4 matPerspective(float fovy, float aspect, float zn, float zf) {
    Mat4 r;
    float f = 1.0f / tanf(fovy * 0.5f);
    for (float& v : r.m) v = 0;
    r.m[0] = f / aspect; r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1;
    r.m[14] = 2 * zf * zn / (zn - zf);
    return r;
}
inline Mat4 matOrtho(float l, float r_, float b, float t_, float zn, float zf) {
    Mat4 o;
    for (float& v : o.m) v = 0;
    o.m[0] = 2 / (r_ - l); o.m[5] = 2 / (t_ - b); o.m[10] = -2 / (zf - zn);
    o.m[12] = -(r_ + l) / (r_ - l); o.m[13] = -(t_ + b) / (t_ - b); o.m[14] = -(zf + zn) / (zf - zn);
    o.m[15] = 1;
    return o;
}
inline Mat4 matLookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = norm(center - eye);
    Vec3 s = norm(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    return r;
}

// ---------------------------------------------------------------- RNG helpers
extern std::mt19937 g_rng;
inline float randf(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(g_rng);
}
inline int randi(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(g_rng);
}

// ---------------------------------------------------------------- Misc
struct Color {
    static Vec3 cyan() { return {0.31f, 0.85f, 0.92f}; }
    static Vec3 amber() { return {1.0f, 0.71f, 0.28f}; }
    static Vec3 red() { return {1.0f, 0.28f, 0.34f}; }
    static Vec3 green() { return {0.42f, 0.87f, 0.48f}; }
    static Vec3 magenta() { return {0.95f, 0.36f, 0.85f}; }
    static Vec3 yellow() { return {1.0f, 0.9f, 0.3f}; }
    static Vec3 white() { return {1, 1, 1}; }
};

// Forward declaration so every module can see it.
struct Game;
