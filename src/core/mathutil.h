#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    float x, y, w, h;
} Rect;

static inline Vec2 vec2(float x, float y) {
    return (Vec2){x, y};
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

static inline Vec2 vec2_scale(Vec2 v, float s) {
    return (Vec2){v.x * s, v.y * s};
}

static inline float vec2_length(Vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static inline float vec2_length_sq(Vec2 v) {
    return v.x * v.x + v.y * v.y;
}

static inline Vec2 vec2_normalize(Vec2 v) {
    float len = vec2_length(v);
    if (len < 0.0001f) return (Vec2){0, 0};
    return (Vec2){v.x / len, v.y / len};
}

static inline float vec2_distance(Vec2 a, Vec2 b) {
    return vec2_length(vec2_sub(a, b));
}

static inline float vec2_distance_sq(Vec2 a, Vec2 b) {
    return vec2_length_sq(vec2_sub(a, b));
}

static inline float vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline float vec2_angle(Vec2 v) {
    return atan2f(v.y, v.x);
}

static inline Vec2 vec2_from_angle(float angle, float length) {
    return (Vec2){cosf(angle) * length, sinf(angle) * length};
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float clampf(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

static inline int clampi(int val, int min, int max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

/* Closest point on segment [a,b] to point p. */
static inline Vec2 vec2_closest_on_segment(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 ab = vec2_sub(b, a);
    float len2 = vec2_length_sq(ab);
    if (len2 < 0.0001f) return a;
    float t = clampf(vec2_dot(vec2_sub(p, a), ab) / len2, 0.0f, 1.0f);
    return vec2_add(a, vec2_scale(ab, t));
}

static inline bool circle_circle_collision(Vec2 pos_a, float radius_a, Vec2 pos_b, float radius_b) {
    float dist_sq = vec2_distance_sq(pos_a, pos_b);
    float radii = radius_a + radius_b;
    return dist_sq <= radii * radii;
}

static inline bool point_in_rect(Vec2 point, Rect rect) {
    return point.x >= rect.x && point.x <= rect.x + rect.w &&
           point.y >= rect.y && point.y <= rect.y + rect.h;
}

#endif
