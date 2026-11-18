using UnityEngine;

namespace SLInternal
{
    // The camera used for world-to-screen. Prefers Camera.main; otherwise the enabled
    // screen camera closest to the local player's eye. Projection goes through viewport
    // space so render scaling and render-texture sizes do not matter.
    internal sealed class ViewCamera
    {
        private Camera[] _all = new Camera[16];
        private int _frame = -1;

        public Camera Current { get; private set; }

        public string Describe =>
            Current == null ? "none" : $"{Current.name} (fov {Current.fieldOfView:0.#}, {(Current == Camera.main ? "Camera.main" : "nearest to eye")})";

        public Camera Get(WorldCache world)
        {
            if (_frame == Time.frameCount)
                return Current;
            _frame = Time.frameCount;

            Camera main = Camera.main;
            if (main != null && main.isActiveAndEnabled && main.targetTexture == null)
                return Current = main;

            if (Camera.allCamerasCount > _all.Length)
                _all = new Camera[Camera.allCamerasCount + 8];
            int n = Camera.GetAllCameras(_all);
            Camera best = null;
            float bestDist = float.MaxValue;
            Vector3 eye = world.LocalEye;
            for (int i = 0; i < n; i++)
            {
                Camera c = _all[i];
                if (c == null || !c.isActiveAndEnabled || c.targetTexture != null)
                    continue;
                float d = world.LocalAlive ? (c.transform.position - eye).sqrMagnitude : -c.depth;
                if (d < bestDist)
                {
                    bestDist = d;
                    best = c;
                }
            }
            return Current = best;
        }

        // GUI coordinates (origin top-left). False if the point is behind the camera.
        public static bool WorldToGui(Camera cam, Vector3 world, out Vector2 gui)
        {
            Vector3 v = cam.WorldToViewportPoint(world);
            if (!(v.z > 0.01f) || !AimMath.IsFinite(v.x) || !AimMath.IsFinite(v.y))
            {
                gui = default;
                return false;
            }
            gui = new Vector2(v.x * Screen.width, (1f - v.y) * Screen.height);
            return true;
        }

        private static readonly Vector3[] Corners = new Vector3[8];

        public static bool BoundsToGui(Camera cam, Bounds b, out Rect rect)
        {
            Vector3 mn = b.min, mx = b.max;
            Corners[0] = new Vector3(mn.x, mn.y, mn.z);
            Corners[1] = new Vector3(mx.x, mn.y, mn.z);
            Corners[2] = new Vector3(mn.x, mx.y, mn.z);
            Corners[3] = new Vector3(mx.x, mx.y, mn.z);
            Corners[4] = new Vector3(mn.x, mn.y, mx.z);
            Corners[5] = new Vector3(mx.x, mn.y, mx.z);
            Corners[6] = new Vector3(mn.x, mx.y, mx.z);
            Corners[7] = new Vector3(mx.x, mx.y, mx.z);

            float x0 = float.MaxValue, y0 = float.MaxValue, x1 = float.MinValue, y1 = float.MinValue;
            for (int i = 0; i < 8; i++)
            {
                if (!WorldToGui(cam, Corners[i], out Vector2 p))
                {
                    rect = default;
                    return false;
                }
                x0 = Mathf.Min(x0, p.x); y0 = Mathf.Min(y0, p.y);
                x1 = Mathf.Max(x1, p.x); y1 = Mathf.Max(y1, p.y);
            }
            rect = Rect.MinMaxRect(x0, y0, x1, y1);
            // Reject degenerate or absurd projections (e.g. a corner right at the near plane).
            return rect.width >= 1f && rect.height >= 1f && rect.width < Screen.width * 4f && rect.height < Screen.height * 4f;
        }
    }
}
