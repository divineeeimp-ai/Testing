using System;

namespace SLInternal
{
    // Pure math, no Unity types, so it can be checked outside the game.
    // Unity convention: left-handed, +Y up, +Z forward, degrees.
    // Yaw 0 looks along +Z and grows toward +X (turning right). Pitch is positive looking up.
    public static class AimMath
    {
        public const float Rad2Deg = 57.29577951308232f;

        public static bool IsFinite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);

        public static float NormalizeDeg(float a)
        {
            a %= 360f;
            if (a > 180f) a -= 360f;
            else if (a <= -180f) a += 360f;
            return a;
        }

        public static bool DirToAngles(float x, float y, float z, out float yaw, out float pitch)
        {
            float len = (float)Math.Sqrt(x * x + y * y + z * z);
            if (!IsFinite(len) || len < 1e-6f)
            {
                yaw = pitch = 0f;
                return false;
            }
            yaw = (float)Math.Atan2(x, z) * Rad2Deg;
            pitch = (float)Math.Asin(Clamp(y / len, -1f, 1f)) * Rad2Deg;
            return true;
        }

        public static float AngleBetweenDeg(float ax, float ay, float az, float bx, float by, float bz)
        {
            float la = (float)Math.Sqrt(ax * ax + ay * ay + az * az);
            float lb = (float)Math.Sqrt(bx * bx + by * by + bz * bz);
            if (la < 1e-6f || lb < 1e-6f || !IsFinite(la) || !IsFinite(lb))
                return 180f;
            float cos = (ax * bx + ay * by + az * bz) / (la * lb);
            return (float)Math.Acos(Clamp(cos, -1f, 1f)) * Rad2Deg;
        }

        // Fraction of the remaining error to remove this frame; frame-rate independent.
        public static float StepFraction(float speedPerSecond, float dt)
        {
            if (!(speedPerSecond > 0f) || !(dt > 0f))
                return 0f;
            return Clamp(1f - (float)Math.Exp(-speedPerSecond * dt), 0f, 1f);
        }

        public static float ClampAbs(float v, float maxAbs) => maxAbs > 0f ? Clamp(v, -maxAbs, maxAbs) : v;

        // Screen-space radius of a cone with half-angle fovDeg around the view axis,
        // for a camera with the given vertical field of view.
        public static float FovRadiusPixels(float fovDeg, float cameraVerticalFovDeg, float screenHeight)
        {
            if (!(cameraVerticalFovDeg > 1f) || cameraVerticalFovDeg >= 179f || !(fovDeg > 0f))
                return 0f;
            double half = cameraVerticalFovDeg * 0.5 / Rad2Deg;
            double cone = Math.Min(fovDeg, 89.0) / Rad2Deg;
            return (float)(Math.Tan(cone) / Math.Tan(half) * screenHeight * 0.5);
        }

        public static float Clamp(float v, float min, float max) => v < min ? min : v > max ? max : v;
    }

    // Converts fractional mouse counts to integers without losing the remainder.
    public struct MouseAccumulator
    {
        private float _rx, _ry;

        public void Add(float x, float y, out int dx, out int dy)
        {
            if (!AimMath.IsFinite(x)) x = 0f;
            if (!AimMath.IsFinite(y)) y = 0f;
            _rx += x;
            _ry += y;
            dx = (int)Math.Truncate(_rx);
            dy = (int)Math.Truncate(_ry);
            _rx -= dx;
            _ry -= dy;
        }

        public void Reset() => _rx = _ry = 0f;
    }
}
