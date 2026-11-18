#pragma once
// Pure screen-space math. No game or Windows dependencies, so it can be checked outside the game.
//
// Conventions (Unity):
//  - World space: left-handed, Y up, metres.
//  - Matrix4x4 memory order is column-major: m00, m10, m20, m30, m01, ... -> element(row, col) = m[col * 4 + row].
//    (validated at runtime from the field offsets of UnityEngine.Matrix4x4).
//  - Camera.worldToCameraMatrix maps into OpenGL-style camera space (camera looks down -Z).
//  - Camera.projectionMatrix is OpenGL-style; clip.w = -z_camera for a perspective camera.
//  - Viewport: x, y in [0, 1], origin bottom-left; z = distance in front of the camera.
//  - IMGUI: pixels, origin top-left, y down.

#include <cmath>

namespace slm {

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Mat4 { float m[16]; };

inline bool Finite(float f) { return std::isfinite(f); }
inline bool Finite(Vec3 v) { return Finite(v.x) && Finite(v.y) && Finite(v.z); }

inline bool Sane(Vec3 v, float limit = 20000.0f)
{
    return Finite(v) && std::fabs(v.x) < limit && std::fabs(v.y) < limit && std::fabs(v.z) < limit;
}

inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline float Length(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline float Distance(Vec3 a, Vec3 b) { return Length(Sub(a, b)); }

inline float At(const Mat4& a, int row, int col) { return a.m[col * 4 + row]; }

inline void Mul(const Mat4& a, const float v[4], float out[4])
{
    for (int r = 0; r < 4; ++r)
        out[r] = At(a, r, 0) * v[0] + At(a, r, 1) * v[1] + At(a, r, 2) * v[2] + At(a, r, 3) * v[3];
}

// Same result as Camera.WorldToViewportPoint for a perspective camera. False when the point is
// behind the camera or the result is not finite.
inline bool WorldToViewport(const Mat4& view, const Mat4& proj, Vec3 p, Vec3& viewport)
{
    const float world[4] = {p.x, p.y, p.z, 1.0f};
    float cam[4];
    Mul(view, world, cam);
    float clip[4];
    Mul(proj, cam, clip);
    if (!(clip[3] > 1e-6f))
        return false;
    viewport = {(clip[0] / clip[3]) * 0.5f + 0.5f, (clip[1] / clip[3]) * 0.5f + 0.5f, -cam[2]};
    return Finite(viewport);
}

constexpr float kMaxViewportExtent = 4.0f;

inline bool ViewportToGui(Vec3 vp, float nearClip, float screenW, float screenH, Vec2& gui)
{
    if (!Finite(vp) || vp.z < nearClip || screenW < 1.0f || screenH < 1.0f)
        return false;
    if (std::fabs(vp.x) > kMaxViewportExtent || std::fabs(vp.y) > kMaxViewportExtent)
        return false;
    gui = {vp.x * screenW, (1.0f - vp.y) * screenH};
    return true;
}

struct Rect2 { float x, y, w, h; };

// 2D box from the projected top and bottom of a vertical capsule; widthToHeight = (2 * radius) / height.
inline bool BoxFromVertical(Vec2 top, Vec2 bottom, float widthToHeight, Rect2& box)
{
    const float h = bottom.y - top.y;
    if (!Finite(h) || h < 2.0f || !Finite(widthToHeight) || widthToHeight <= 0.0f)
        return false;
    float w = h * widthToHeight;
    if (w < 2.0f)
        w = 2.0f;
    box = {(top.x + bottom.x) * 0.5f - w * 0.5f, top.y, w, h};
    return true;
}

inline bool IntersectsScreen(Rect2 r, float screenW, float screenH)
{
    return r.x + r.w >= 0.0f && r.y + r.h >= 0.0f && r.x <= screenW && r.y <= screenH;
}

} // namespace slm
