using UnityEngine;

namespace SLInternal
{
    // Presses the left mouse button while the crosshair ray (the game's hitreg mask,
    // from the local camera) hits an enemy hitbox first.
    internal sealed class AutoShoot
    {
        private readonly Settings _s;
        private readonly WorldCache _world;
        private bool _pressed;
        private float _onTargetSince = -1f;
        private float _pressedAt;
        private float _nextAllowed;

        public bool OnTarget { get; private set; }

        public AutoShoot(Settings s, WorldCache world)
        {
            _s = s;
            _world = world;
        }

        public void Release()
        {
            if (_pressed)
                Win32.LeftButton(false);
            _pressed = false;
            _onTargetSince = -1f;
            OnTarget = false;
        }

        public void Update(float now, bool menuOpen)
        {
            bool active = _s.TrigEnabled && !menuOpen && _world.LocalAlive && Win32.GameFocused() &&
                          (!_s.TrigRequireKey || Win32.KeyDown(_s.TrigKey));
            if (!active)
            {
                Release();
                return;
            }

            // The player is already holding fire: don't inject on top of it.
            if (!_pressed && Win32.KeyDown(Win32.VK_LBUTTON))
            {
                _onTargetSince = -1f;
                OnTarget = false;
                return;
            }

            OnTarget = false;
            if (_world.Raycast(_world.LocalEye, _world.LocalForward, _s.TrigMaxDistance, out ReferenceHub hub) == RayResult.Player)
            {
                PlayerInfo p = _world.Find(hub);
                OnTarget = p != null && (p.Enemy || _s.TrigTeammates);
            }

            if (OnTarget) { if (_onTargetSince < 0f) _onTargetSince = now; }
            else _onTargetSince = -1f;

            if (!_pressed)
            {
                if (OnTarget && now - _onTargetSince >= _s.TrigDelayMs * 0.001f && now >= _nextAllowed && Win32.LeftButton(true))
                {
                    _pressed = true;
                    _pressedAt = now;
                }
                return;
            }

            bool done = _s.TrigHoldWhileOnTarget ? !OnTarget : now - _pressedAt >= _s.TrigHoldMs * 0.001f;
            if (done)
            {
                Win32.LeftButton(false);
                _pressed = false;
                _nextAllowed = now + _s.TrigIntervalMs * 0.001f;
            }
        }
    }
}
