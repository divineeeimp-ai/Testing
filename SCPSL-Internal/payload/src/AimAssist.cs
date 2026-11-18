using UnityEngine;

namespace SLInternal
{
    // Legit aim correction: while the aim key is held, the view is pulled toward the
    // best target inside the FOV cone with exponential smoothing and a speed cap.
    // Output is relative mouse movement (SendInput), converted with the measured
    // degrees-per-count; the game's own mouse look turns the camera.
    internal sealed class AimAssist
    {
        private readonly Settings _s;
        private readonly WorldCache _world;
        private MouseAccumulator _acc;
        private ReferenceHub _target;

        public ReferenceHub Target => _target;

        public AimAssist(Settings s, WorldCache world)
        {
            _s = s;
            _world = world;
        }

        public void Reset()
        {
            _target = null;
            _acc.Reset();
        }

        public Vector3 AimPoint(PlayerInfo p) => Vector3.Lerp(p.Position, p.Eye, Mathf.Clamp01(_s.AimHeight));

        public void Update(float dt, bool menuOpen)
        {
            if (!_s.AimEnabled || !_s.Calibrated || menuOpen || !_world.LocalAlive ||
                !Win32.GameFocused() || !Win32.KeyDown(_s.AimKey) || !(dt > 0f) || dt > 0.25f)
            {
                Reset();
                return;
            }

            Vector3 eye = _world.LocalEye;
            Vector3 fwd = _world.LocalForward;
            if (!AimMath.DirToAngles(fwd.x, fwd.y, fwd.z, out float curYaw, out float curPitch))
            {
                Reset();
                return;
            }

            PlayerInfo best = null;
            Vector3 bestPoint = default;
            float bestAngle = float.MaxValue;

            if (_s.AimStickyTarget && _target != null)
            {
                PlayerInfo t = _world.Find(_target);
                if (Valid(t, eye, fwd, out Vector3 pt, out float ang))
                {
                    best = t;
                    bestPoint = pt;
                }
            }

            if (best == null)
            {
                foreach (PlayerInfo p in _world.Players)
                {
                    if (Valid(p, eye, fwd, out Vector3 pt, out float ang) && ang < bestAngle)
                    {
                        best = p;
                        bestPoint = pt;
                        bestAngle = ang;
                    }
                }
            }

            if (best == null)
            {
                Reset();
                return;
            }
            if (best.Hub != _target)
                _acc.Reset();
            _target = best.Hub;

            Vector3 to = bestPoint - eye;
            if (!AimMath.DirToAngles(to.x, to.y, to.z, out float wantYaw, out float wantPitch))
                return;

            float errYaw = AimMath.NormalizeDeg(wantYaw - curYaw);
            float errPitch = wantPitch - curPitch;
            if (Mathf.Abs(errYaw) < _s.AimDeadzone && Mathf.Abs(errPitch) < _s.AimDeadzone)
                return;

            float frac = AimMath.StepFraction(_s.AimSpeed, dt);
            float cap = _s.AimMaxDegPerSec * dt;
            float stepYaw = AimMath.ClampAbs(errYaw * frac, cap);
            float stepPitch = AimMath.ClampAbs(errPitch * frac, cap);

            _acc.Add(stepYaw / _s.DegPerCountX, stepPitch / _s.DegPerCountY, out int dx, out int dy);
            if (dx != 0 || dy != 0)
                Win32.MoveMouse(dx, dy);
        }

        private bool Valid(PlayerInfo p, Vector3 eye, Vector3 fwd, out Vector3 point, out float angle)
        {
            point = default;
            angle = 180f;
            if (p == null || p.Hub == null || (!p.Enemy && !_s.AimTeammates) || p.Distance > _s.AimMaxDistance)
                return false;
            point = AimPoint(p);
            Vector3 to = point - eye;
            angle = AimMath.AngleBetweenDeg(fwd.x, fwd.y, fwd.z, to.x, to.y, to.z);
            if (angle > _s.AimFov)
                return false;
            return !_s.AimVisibleOnly || _world.CanSee(p, point);
        }

        public void DrawFov(Camera cam, bool menuOpen)
        {
            if (!_s.AimEnabled || !_s.AimDrawFov || cam == null || !_world.LocalAlive)
                return;
            float r = AimMath.FovRadiusPixels(_s.AimFov, cam.fieldOfView, Screen.height);
            var c = new Color(1f, 1f, 1f, _target != null ? 0.8f : 0.35f);
            if (!_s.Calibrated)
                c = new Color(1f, 0.3f, 0.3f, 0.6f);
            Draw.Circle(new Vector2(Screen.width * 0.5f, Screen.height * 0.5f), r, c);
        }
    }

    // Measures how many degrees one mouse count turns the view, by injecting a known
    // movement and reading the local camera's yaw/pitch before and after.
    internal sealed class Calibrator
    {
        private const int CountsX = 100;
        private const int CountsY = 60;
        private const float SettleSeconds = 0.4f;
        private const float MaxAbsPitchForTest = 50f;

        private enum Phase { Idle, Settle, MeasureX, SettleY, MeasureY }

        private readonly Settings _s;
        private readonly WorldCache _world;
        private Phase _phase;
        private float _until;
        private float _yaw0, _pitch0, _degX;

        public bool Running => _phase != Phase.Idle;
        public string Message { get; private set; } = "not calibrated";

        public Calibrator(Settings s, WorldCache world)
        {
            _s = s;
            _world = world;
            if (s.Calibrated)
                Message = $"loaded: {s.DegPerCountX:0.#####} / {s.DegPerCountY:0.#####} deg per count";
        }

        public void Start()
        {
            if (!_world.LocalAlive)
            {
                Message = "spawn as a first-person role first";
                return;
            }
            _phase = Phase.Settle;
            _until = Time.unscaledTime + SettleSeconds;
            Message = "calibrating: don't touch the mouse";
        }

        private bool Angles(out float yaw, out float pitch)
        {
            Vector3 f = _world.LocalForward;
            return AimMath.DirToAngles(f.x, f.y, f.z, out yaw, out pitch);
        }

        private void Fail(string why)
        {
            _phase = Phase.Idle;
            Message = "calibration failed: " + why;
            Log.Info(Message);
        }

        public void Update()
        {
            if (_phase == Phase.Idle)
                return;
            if (!_world.LocalAlive || !Win32.GameFocused())
            {
                Fail("lost focus or the local player is not alive");
                return;
            }
            if (Time.unscaledTime < _until)
                return;
            if (!Angles(out float yaw, out float pitch))
            {
                Fail("view direction unreadable");
                return;
            }

            switch (_phase)
            {
                case Phase.Settle:
                    _yaw0 = yaw;
                    Win32.MoveMouse(CountsX, 0);
                    Next(Phase.MeasureX);
                    break;
                case Phase.MeasureX:
                    _degX = AimMath.NormalizeDeg(yaw - _yaw0) / CountsX;
                    Win32.MoveMouse(-CountsX, 0);
                    Next(Phase.SettleY);
                    break;
                case Phase.SettleY:
                    if (Mathf.Abs(pitch) > MaxAbsPitchForTest)
                    {
                        Fail("look closer to the horizon and retry");
                        return;
                    }
                    _pitch0 = pitch;
                    Win32.MoveMouse(0, CountsY);
                    Next(Phase.MeasureY);
                    break;
                case Phase.MeasureY:
                    float degY = (pitch - _pitch0) / CountsY;
                    Win32.MoveMouse(0, -CountsY);
                    _phase = Phase.Idle;
                    if (!Plausible(_degX) || !Plausible(degY))
                    {
                        Fail($"measured {_degX:0.######} / {degY:0.######} deg per count; injected input may be ignored");
                        return;
                    }
                    _s.DegPerCountX = _degX;
                    _s.DegPerCountY = degY;
                    Message = $"measured {_degX:0.#####} / {degY:0.#####} deg per count (save to keep)";
                    Log.Info(Message);
                    break;
            }
        }

        private void Next(Phase p)
        {
            _phase = p;
            _until = Time.unscaledTime + SettleSeconds;
        }

        private static bool Plausible(float v) => AimMath.IsFinite(v) && Mathf.Abs(v) > 1e-4f && Mathf.Abs(v) < 2f;
    }
}
