using UnityEngine;

namespace SLMenu.Render
{
    /// <summary>
    /// Finds the camera the world is rendered through. The game tracks its active camera as a Transform
    /// (MainCameraController.CurrentCamera); which Camera component sits on it is prefab data that could
    /// not be read from the assembly, so both candidates are tried and the menu shows which one was used
    /// and how far it is from MainCameraController.LastPosition.
    /// </summary>
    internal static class ViewCamera
    {
        public static string Source { get; private set; } = "none";
        public static float OffsetFromGameCamera { get; private set; } = -1f;

        public static Camera Resolve()
        {
            Camera cam = Camera.main;
            if (cam != null && cam.isActiveAndEnabled)
            {
                Source = "Camera.main (" + cam.name + ")";
            }
            else
            {
                cam = null;
                Transform current = MainCameraController.CurrentCamera;
                if (current != null)
                {
                    Camera child = current.GetComponentInChildren<Camera>(false);
                    if (child != null && child.isActiveAndEnabled)
                    {
                        cam = child;
                        Source = "MainCameraController.CurrentCamera (" + child.name + ")";
                    }
                }
            }

            if (cam == null)
            {
                Source = "none";
                OffsetFromGameCamera = -1f;
                return null;
            }

            OffsetFromGameCamera = Vector3.Distance(cam.transform.position, MainCameraController.LastPosition);
            return cam;
        }

        public static bool WorldToGui(Camera cam, Vector3 world, out Vector2 gui)
        {
            Vector3 vp = cam.WorldToViewportPoint(world);
            bool ok = ScreenMath.ViewportToGui(vp.x, vp.y, vp.z, cam.nearClipPlane, Screen.width, Screen.height,
                                               out float gx, out float gy);
            gui = new Vector2(gx, gy);
            return ok;
        }
    }
}
