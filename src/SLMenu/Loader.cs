using System;
using System.Threading;
using UnityEngine;
using Object = UnityEngine.Object;

namespace SLMenu
{
    /// <summary>
    /// Entry points for a Mono injector: namespace SLMenu, class Loader, methods Load / Unload
    /// (static, no arguments). Injectors usually invoke these on their own thread, while Unity objects
    /// may only be created on the main thread, so the work is deferred to the game's
    /// StaticUnityMethods.OnUpdate event, which the game raises from a MonoBehaviour.Update.
    /// </summary>
    public static class Loader
    {
        private static GameObject _root;
        private static int _createPending;
        private static int _destroyPending;

        public static void Load()
        {
            if (Interlocked.Exchange(ref _createPending, 1) == 1)
                return;
            try
            {
                SubscribeCreate();
            }
            catch (Exception e)
            {
                _createPending = 0;
                Debug.LogError("[SLMenu] Could not hook StaticUnityMethods.OnUpdate: " + e);
            }
        }

        public static void Unload()
        {
            if (Interlocked.Exchange(ref _destroyPending, 1) == 1)
                return;
            try
            {
                SubscribeDestroy();
            }
            catch (Exception e)
            {
                _destroyPending = 0;
                Debug.LogError("[SLMenu] Could not hook StaticUnityMethods.OnUpdate: " + e);
            }
        }

        // Separate methods so a missing StaticUnityMethods fails inside Load's try block when JIT-compiled.
        private static void SubscribeCreate()
        {
            StaticUnityMethods.OnUpdate += CreateOnMainThread;
        }

        private static void SubscribeDestroy()
        {
            StaticUnityMethods.OnUpdate += DestroyOnMainThread;
        }

        private static void CreateOnMainThread()
        {
            StaticUnityMethods.OnUpdate -= CreateOnMainThread;
            try
            {
                if (_root != null)
                    return;
                var go = new GameObject("SLMenu") { hideFlags = HideFlags.HideAndDontSave };
                Object.DontDestroyOnLoad(go);
                go.AddComponent<MenuBehaviour>();
                _root = go;
                Debug.Log("[SLMenu] Loaded.");
            }
            catch (Exception e)
            {
                Debug.LogError("[SLMenu] Load failed: " + e);
            }
            finally
            {
                _createPending = 0;
            }
        }

        private static void DestroyOnMainThread()
        {
            StaticUnityMethods.OnUpdate -= DestroyOnMainThread;
            try
            {
                if (_root != null)
                    Object.Destroy(_root);
                _root = null;
                Debug.Log("[SLMenu] Unloaded.");
            }
            catch (Exception e)
            {
                Debug.LogError("[SLMenu] Unload failed: " + e);
            }
            finally
            {
                _destroyPending = 0;
            }
        }
    }
}
