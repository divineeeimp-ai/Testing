using System;
using System.Threading;
using UnityEngine;
using UnityEngine.Rendering;
using Object = UnityEngine.Object;

namespace SLInternal
{
    // Entry point for the native bootstrap. Init() runs on the bootstrap's own thread
    // (attached to Mono, but not Unity's main thread), so it only subscribes to
    // callbacks that Unity raises on the main thread and creates the GameObject there.
    public static class Loader
    {
        private static int _state; // 0 idle, 1 waiting for main thread, 2 running
        private static GameObject _root;

        public static void Init()
        {
            Log.Init(Environment.GetEnvironmentVariable("SLINTERNAL_DIR"));
            if (Interlocked.CompareExchange(ref _state, 1, 0) != 0)
            {
                Log.Info("Init ignored: already loaded or loading");
                return;
            }

            // Whichever fires first wins: onBeforeRender (any pipeline),
            // beginCameraRendering (SRP/HDRP), onPreCull (built-in pipeline).
            Application.onBeforeRender += OnBeforeRender;
            RenderPipelineManager.beginCameraRendering += OnBeginCameraRendering;
            Camera.onPreCull += OnPreCull;
            Log.Info("Init: waiting for a main-thread callback");
        }

        public static void Unload()
        {
            if (_root != null)
                Object.Destroy(_root);
        }

        internal static void OnMainDestroyed()
        {
            _root = null;
            Interlocked.Exchange(ref _state, 0);
            Log.Info("Unloaded. The assembly stays in the Mono domain; inject again to reload it.");
        }

        // Called from Main.Start, i.e. outside of the callbacks being removed.
        internal static void DetachMainThreadCallbacks()
        {
            Application.onBeforeRender -= OnBeforeRender;
            RenderPipelineManager.beginCameraRendering -= OnBeginCameraRendering;
            Camera.onPreCull -= OnPreCull;
        }

        private static void OnBeforeRender() => CreateOnMainThread();

        private static void OnBeginCameraRendering(ScriptableRenderContext context, Camera camera) => CreateOnMainThread();

        private static void OnPreCull(Camera camera) => CreateOnMainThread();

        private static void CreateOnMainThread()
        {
            if (Interlocked.CompareExchange(ref _state, 2, 1) != 1)
                return;
            try
            {
                _root = new GameObject("SLInternal");
                _root.hideFlags = HideFlags.HideAndDontSave;
                Object.DontDestroyOnLoad(_root);
                _root.AddComponent<Main>();
                Log.Info("Main component created on the main thread");
            }
            catch (Exception e)
            {
                Log.Error("creating Main failed: " + e);
                if (_root != null)
                    Object.Destroy(_root);
                _root = null;
                DetachMainThreadCallbacks();
                Interlocked.Exchange(ref _state, 0);
            }
        }
    }
}
