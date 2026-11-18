using System;
using System.Collections.Generic;
using PlayerRoles;
using UnityEngine;

namespace SLInternal
{
    internal sealed class PlayerInfo
    {
        public ReferenceHub Hub;
        public string Name;
        public RoleTypeId Role;
        public Team Team;
        public bool Enemy;
        public Vector3 Position; // FpcModule.Position
        public Vector3 Eye;      // PlayerCameraReference.position
        public Bounds Box;
        public bool BoxFromController;
        public float Health, MaxHealth;
        public bool HealthValid;
        public float Distance;
    }

    internal enum RayResult { Clear, Player, Blocked }

    // Per-frame snapshot of players, rebuilt in LateUpdate on the main thread.
    internal sealed class WorldCache
    {
        private const int MaxPlayers = 256;
        private const float MaxCoordinate = 20000f;

        public ReferenceHub Local;
        public bool LocalAlive;
        public RoleTypeId LocalRole;
        public Team LocalTeam;
        public Vector3 LocalEye;
        public Vector3 LocalForward;

        public readonly List<PlayerInfo> Players = new List<PlayerInfo>(64);
        private readonly List<PlayerInfo> _pool = new List<PlayerInfo>(64);
        private readonly Dictionary<ReferenceHub, PlayerInfo> _byHub = new Dictionary<ReferenceHub, PlayerInfo>(64);
        private readonly Dictionary<ReferenceHub, CharacterController> _controllers = new Dictionary<ReferenceHub, CharacterController>(64);
        private readonly Dictionary<RoleTypeId, string> _roleNames = new Dictionary<RoleTypeId, string>();
        private readonly RaycastHit[] _hits = new RaycastHit[48];
        private static readonly IComparer<RaycastHit> ByDistance = Comparer<RaycastHit>.Create((a, b) => a.distance.CompareTo(b.distance));
        private int _controllerPurgeFrame;

        public PlayerInfo Find(ReferenceHub hub) => hub != null && _byHub.TryGetValue(hub, out PlayerInfo p) ? p : null;

        public string RoleName(RoleTypeId role)
        {
            if (!_roleNames.TryGetValue(role, out string s))
                _roleNames[role] = s = role.ToString();
            return s;
        }

        public void Refresh()
        {
            _pool.AddRange(Players);
            Players.Clear();
            _byHub.Clear();
            Local = null;
            LocalAlive = false;

            HashSet<ReferenceHub> all = GameApi.AllHubs;
            if (all == null)
                return;

            foreach (ReferenceHub hub in all)
            {
                if (hub != null && GameApi.IsLocal(hub))
                {
                    Local = hub;
                    break;
                }
            }
            if (Local == null)
                return;

            PlayerRoleBase localRole = GameApi.CurrentRole(Local);
            Transform localCam = GameApi.CameraTransform(Local);
            if (localRole != null && localCam != null)
            {
                LocalRole = localRole.RoleTypeId;
                LocalTeam = localRole.Team;
                LocalEye = localCam.position;
                LocalForward = localCam.forward;
                LocalAlive = LocalTeam != Team.Dead && GameApi.TryGetFpcPosition(localRole, out _) && Plausible(LocalEye);
            }

            Vector3 viewOrigin = localCam != null ? localCam.position : Local.transform.position;
            int count = 0;
            foreach (ReferenceHub hub in all)
            {
                if (++count > MaxPlayers)
                    break;
                if (hub == null || hub == Local)
                    continue;
                PlayerRoleBase role = GameApi.CurrentRole(hub);
                if (role == null)
                    continue;
                Team team = role.Team;
                if (team == Team.Dead || !GameApi.TryGetFpcPosition(role, out Vector3 pos) || !Plausible(pos))
                    continue;

                Transform cam = GameApi.CameraTransform(hub);
                Vector3 eye = cam != null ? cam.position : pos;
                if (!Plausible(eye))
                    eye = pos;

                PlayerInfo p = Rent();
                p.Hub = hub;
                p.Name = GameApi.DisplayName(hub) ?? "?";
                p.Role = role.RoleTypeId;
                p.Team = team;
                p.Position = pos;
                p.Eye = eye;
                p.Enemy = LocalAlive && GameApi.IsEnemy(LocalRole, LocalTeam, p.Role, team);
                p.HealthValid = GameApi.TryGetHealth(hub, out p.Health, out p.MaxHealth);
                p.Distance = Vector3.Distance(viewOrigin, pos);
                BuildBox(p);
                Players.Add(p);
                _byHub[hub] = p;
            }

            if (Time.frameCount - _controllerPurgeFrame > 600)
            {
                _controllerPurgeFrame = Time.frameCount;
                PurgeDeadControllers();
            }
        }

        private void BuildBox(PlayerInfo p)
        {
            CharacterController cc = Controller(p.Hub);
            if (cc != null && cc.enabled)
            {
                Bounds b = cc.bounds;
                if (b.size.y > 0.3f && b.size.y < 8f && Plausible(b.center))
                {
                    p.Box = b;
                    p.BoxFromController = true;
                    return;
                }
            }
            // Fallback when there is no usable controller: a column from below the body
            // position up past the eye. The proportions are an approximation, not game data.
            float eyeAbove = Mathf.Max(0.2f, p.Eye.y - p.Position.y);
            float bottom = p.Position.y - eyeAbove;
            float top = p.Eye.y + eyeAbove * 0.15f;
            float width = (top - bottom) * 0.35f;
            p.Box = new Bounds(new Vector3(p.Position.x, (top + bottom) * 0.5f, p.Position.z), new Vector3(width, top - bottom, width));
            p.BoxFromController = false;
        }

        private CharacterController Controller(ReferenceHub hub)
        {
            if (!_controllers.TryGetValue(hub, out CharacterController cc) || cc == null)
            {
                cc = hub.GetComponent<CharacterController>();
                _controllers[hub] = cc;
            }
            return cc;
        }

        private readonly List<ReferenceHub> _deadKeys = new List<ReferenceHub>();

        private void PurgeDeadControllers()
        {
            _deadKeys.Clear();
            foreach (KeyValuePair<ReferenceHub, CharacterController> kv in _controllers)
                if (kv.Key == null)
                    _deadKeys.Add(kv.Key);
            foreach (ReferenceHub k in _deadKeys)
                _controllers.Remove(k);
        }

        private PlayerInfo Rent()
        {
            if (_pool.Count == 0)
                return new PlayerInfo();
            PlayerInfo p = _pool[_pool.Count - 1];
            _pool.RemoveAt(_pool.Count - 1);
            return p;
        }

        public static bool Plausible(Vector3 v) =>
            AimMath.IsFinite(v.x) && AimMath.IsFinite(v.y) && AimMath.IsFinite(v.z) &&
            Math.Abs(v.x) < MaxCoordinate && Math.Abs(v.y) < MaxCoordinate && Math.Abs(v.z) < MaxCoordinate;

        // First thing the ray meets with the game's own hit-registration mask.
        // The local player's colliders and non-hitbox player colliders are skipped.
        public RayResult Raycast(Vector3 origin, Vector3 direction, float distance, out ReferenceHub hub)
        {
            hub = null;
            if (!(distance > 0f))
                return RayResult.Clear;
            int n = Physics.RaycastNonAlloc(origin, direction, _hits, distance, GameApi.RaycastMask, QueryTriggerInteraction.UseGlobal);
            if (n <= 0)
                return RayResult.Clear;
            n = Math.Min(n, _hits.Length);
            Array.Sort(_hits, 0, n, ByDistance);
            for (int i = 0; i < n; i++)
            {
                Collider c = _hits[i].collider;
                if (c == null)
                    continue;
                ReferenceHub h = GameApi.HubFromCollider(c, out bool isHitbox);
                if (h != null)
                {
                    if (h == Local || !isHitbox)
                        continue;
                    hub = h;
                    return RayResult.Player;
                }
                return RayResult.Blocked;
            }
            return RayResult.Clear;
        }

        // True if nothing but the target (or nothing at all) lies between the eye and the point.
        public bool CanSee(PlayerInfo target, Vector3 point)
        {
            Vector3 d = point - LocalEye;
            float len = d.magnitude;
            if (len < 0.01f)
                return true;
            RayResult r = Raycast(LocalEye, d / len, len, out ReferenceHub hit);
            return r == RayResult.Clear || (r == RayResult.Player && hit == target.Hub);
        }
    }
}
