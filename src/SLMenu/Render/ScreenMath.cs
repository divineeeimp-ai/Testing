using System;

namespace SLMenu.Render
{
    /// <summary>
    /// Pure screen-space math, independent of Unity so it can be checked outside the game.
    /// Input is a Unity viewport point: x,y in [0,1] with origin bottom-left, z = distance in front of
    /// the camera in world units (negative = behind). Output is IMGUI space: pixels, origin top-left, y down.
    /// </summary>
    internal static class ScreenMath
    {
        /// <summary>Viewport coordinates beyond this are rejected (point is far off-screen or numerically unstable).</summary>
        public const float MaxViewportExtent = 4f;

        public static bool ViewportToGui(float vx, float vy, float depth, float nearClip, float screenW, float screenH,
                                         out float guiX, out float guiY)
        {
            guiX = 0f;
            guiY = 0f;
            if (!IsFinite(vx) || !IsFinite(vy) || !IsFinite(depth))
                return false;
            if (depth < nearClip)
                return false;
            if (screenW < 1f || screenH < 1f)
                return false;
            if (vx < -MaxViewportExtent || vx > MaxViewportExtent || vy < -MaxViewportExtent || vy > MaxViewportExtent)
                return false;

            guiX = vx * screenW;
            guiY = (1f - vy) * screenH;
            return true;
        }

        /// <summary>
        /// Builds a 2D box from the projected top and bottom of a vertical capsule.
        /// widthToHeight is the capsule's (2 * radius) / height in world units.
        /// </summary>
        public static bool BoxFromVertical(float topX, float topY, float bottomX, float bottomY, float widthToHeight,
                                           out float x, out float y, out float w, out float h)
        {
            x = y = w = h = 0f;
            float height = bottomY - topY;
            if (!IsFinite(height) || height < 2f)
                return false;
            if (!IsFinite(widthToHeight) || widthToHeight <= 0f)
                return false;

            w = Math.Max(height * widthToHeight, 2f);
            h = height;
            x = (topX + bottomX) * 0.5f - w * 0.5f;
            y = topY;
            return true;
        }

        public static bool IntersectsScreen(float x, float y, float w, float h, float screenW, float screenH)
        {
            return x + w >= 0f && y + h >= 0f && x <= screenW && y <= screenH;
        }

        public static bool IsFinite(float f)
        {
            return !float.IsNaN(f) && !float.IsInfinity(f);
        }
    }
}
