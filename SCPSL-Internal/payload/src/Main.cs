using System;
using UnityEngine;

namespace SLInternal
{
    public sealed class Main : MonoBehaviour
    {
        private Settings _s;
        private WorldCache _world;
        private ViewCamera _camera;
        private Esp _esp;
        private ItemEsp _items;
        private AimAssist _aim;
        private Calibrator _calibrator;
        private AutoShoot _trigger;
        private Menu _menu;
        private readonly KeyLatch _menuKey = new KeyLatch();
        private readonly KeyLatch _panicKey = new KeyLatch();

        private bool _menuOpen;
        private bool _cursorSaved;
        private CursorLockMode _savedLock;
        private bool _savedVisible;

        internal int PickupCount => _items != null ? _items.Count : 0;
        internal int PlayerCount => _world != null ? _world.Players.Count : 0;
        internal bool TriggerOnTarget => _trigger != null && _trigger.OnTarget;
        internal string CalibrationMessage => _calibrator != null ? _calibrator.Message : string.Empty;
        internal string CameraDescription => _camera != null ? _camera.Describe : "none";

        internal string LocalDescription =>
            _world == null || _world.Local == null ? "not found"
            : _world.LocalAlive ? $"{_world.RoleName(_world.LocalRole)} ({_world.LocalTeam})"
            : "spectating / not first-person";

        private void Awake()
        {
            _s = Settings.Load();
            GameApi.Resolve();
            _world = new WorldCache();
            _camera = new ViewCamera();
            _esp = new Esp(_s, _world);
            _items = new ItemEsp(_s, _world);
            _aim = new AimAssist(_s, _world);
            _calibrator = new Calibrator(_s, _world);
            _trigger = new AutoShoot(_s, _world);
            _menu = new Menu(this, _s);
            try
            {
                _items.Attach();
            }
            catch (Exception e)
            {
                Log.Error("pickup tracking unavailable: " + e);
            }
            Log.Info($"ready: menu {Win32.KeyName(_s.MenuKey)}, unload {Win32.KeyName(_s.PanicKey)}");
        }

        private void Start() => Loader.DetachMainThreadCallbacks();

        private void Update()
        {
            try
            {
                bool focused = Win32.GameFocused();
                if (_menu.Binding)
                {
                    _menu.UpdateBinder();
                    return;
                }
                if (focused && _panicKey.Pressed(_s.PanicKey))
                {
                    Loader.Unload();
                    return;
                }
                if (focused && _menuKey.Pressed(_s.MenuKey))
                    SetMenu(!_menuOpen);
            }
            catch (Exception e)
            {
                Log.Throttled("Update", e);
            }
        }

        private void LateUpdate()
        {
            try
            {
                _world.Refresh();
            }
            catch (Exception e)
            {
                Log.Throttled("WorldCache.Refresh", e);
                _world.Players.Clear();
                _trigger.Release();
                _aim.Reset();
                return;
            }

            try { _items.Update(); }
            catch (Exception e) { Log.Throttled("ItemEsp.Update", e); }

            try
            {
                if (_calibrator.Running)
                {
                    _trigger.Release();
                    _aim.Reset();
                    _calibrator.Update();
                }
                else
                {
                    _aim.Update(Time.unscaledDeltaTime, _menuOpen);
                    _trigger.Update(Time.unscaledTime, _menuOpen);
                }
            }
            catch (Exception e)
            {
                Log.Throttled("aim/trigger", e);
                _trigger.Release();
                _aim.Reset();
            }

            if (_menuOpen)
                ForceCursor();
        }

        private void OnGUI()
        {
            try
            {
                if (Event.current.type == EventType.Repaint)
                {
                    Camera cam = _camera.Get(_world);
                    _items.Draw(cam);
                    _esp.Draw(cam);
                    _aim.DrawFov(cam, _menuOpen);
                }
                if (_menuOpen)
                {
                    ForceCursor();
                    _menu.Draw();
                }
            }
            catch (Exception e)
            {
                Log.Throttled("OnGUI", e);
            }
        }

        internal void StartCalibration()
        {
            SetMenu(false);
            _calibrator.Start();
        }

        private void SetMenu(bool open)
        {
            if (open == _menuOpen)
                return;
            _menuOpen = open;
            if (open)
            {
                _savedLock = Cursor.lockState;
                _savedVisible = Cursor.visible;
                _cursorSaved = true;
                _trigger.Release();
                _aim.Reset();
            }
            else
            {
                RestoreCursor();
            }
        }

        private static void ForceCursor()
        {
            Cursor.lockState = CursorLockMode.None;
            Cursor.visible = true;
        }

        private void RestoreCursor()
        {
            if (!_cursorSaved)
                return;
            Cursor.lockState = _savedLock;
            Cursor.visible = _savedVisible;
            _cursorSaved = false;
        }

        private void OnApplicationFocus(bool hasFocus)
        {
            if (!hasFocus && _trigger != null)
                _trigger.Release();
        }

        private void OnDestroy()
        {
            try
            {
                _trigger?.Release();
                _items?.Detach();
                RestoreCursor();
                Draw.Dispose();
            }
            catch (Exception e)
            {
                Log.Error("OnDestroy: " + e);
            }
            Loader.DetachMainThreadCallbacks();
            Loader.OnMainDestroyed();
        }
    }
}
