using System.Collections.Generic;
using PlayerRoles;
using PlayerRoles.FirstPersonControl;
using PlayerRoles.FirstPersonControl.Thirdperson;
using PlayerStatsSystem;
using SLMenu.Render;
using UnityEngine;

namespace SLMenu.Esp
{
    /// <summary>
    /// Player ESP built from the client's own replicated state.
    /// What the server sends this client (measured in the 14.2.8 assembly):
    /// - Positions: FpcServerPositionDistributor skips targets the receiver's FpcVisibilityController rejects
    ///   (out of range: sqrt(1300) ~ 36 m in the facility, sqrt(4900) = 70 m on Surface; SCP-268, etc.).
    ///   On receive, FpcPositionMessage sets FpcModule.Motor.IsInvisible = true for every FPC player
    ///   not in the packet; their Position is then the last value received.
    /// - HealthStat / HumeShieldStat use SyncMode.PrivateAndSpectators: other players' values reach this
    ///   client only while the local player is dead (Team.Dead), so health is drawn only then.
    /// </summary>
    internal sealed class PlayerEsp
    {
        private const int MaxHubs = 256;
        private const float MaxCoordinate = 20000f;
        private const int MaxNameLength = 32;

        private struct Target
        {
            public Color Color;
            public bool HasBox;
            public Rect Box;
            public bool HasHead;
            public Vector2 Head;
            public Vector2 Anchor; // where labels start (box bottom centre or projected position)
            public string Name;
            public string Info;
            public string Health;
        }

        private readonly List<Target> _targets = new List<Target>(64);

        public int Drawn { get; private set; }
        public int Culled { get; private set; }

        public void Render(Camera cam, Settings s)
        {
            Collect(cam, s);
            var snapOrigin = new Vector2(Screen.width * 0.5f, Screen.height);
            for (int i = 0; i < _targets.Count; i++)
            {
                Target t = _targets[i];
                if (s.PlayerSnaplines)
                    Draw.Line(snapOrigin, t.Anchor, t.Color, 1f);
                if (t.HasBox)
                    Draw.BoxOutline(t.Box, t.Color, 1f);
                if (t.HasHead)
                    Draw.Dot(t.Head, 4f, t.Color);

                float y = t.Anchor.y + 2f;
                y += Draw.Text(t.Anchor.x, y, t.Name, t.Color);
                y += Draw.Text(t.Anchor.x, y, t.Info, t.Color);
                Draw.Text(t.Anchor.x, y, t.Health, t.Color);
            }
        }

        private void Collect(Camera cam, Settings s)
        {
            _targets.Clear();
            Drawn = 0;
            Culled = 0;

            HashSet<ReferenceHub> hubs = ReferenceHub.AllHubs;
            if (hubs == null || hubs.Count > MaxHubs)
                return;

            ReferenceHub.TryGetLocalHub(out ReferenceHub local);
            Team localTeam = Team.Dead;
            if (local != null && local.roleManager != null && local.roleManager.CurrentRole != null)
                localTeam = local.roleManager.CurrentRole.Team;
            bool localAlive = localTeam != Team.Dead;

            Vector3 eye = cam.transform.position;
            float screenW = Screen.width, screenH = Screen.height;

            foreach (ReferenceHub hub in hubs)
            {
                if (hub == null || hub == local)
                    continue;

                PlayerRoleManager roleManager = hub.roleManager;
                if (roleManager == null)
                    continue;
                PlayerRoleBase role = roleManager.CurrentRole;
                if (role == null || !(role is IFpcRole fpcRole))
                    continue; // spectators, SCP-079, unspawned: no body in the world

                FirstPersonMovementModule module = fpcRole.FpcModule;
                if (module == null || !module.ModuleReady)
                    continue;

                Team team = role.Team;
                if (localAlive && !s.ShowTeammates && !HitboxIdentity.IsEnemy(localTeam, team))
                    continue;

                FpcMotor motor = module.Motor;
                bool culled = motor != null && motor.IsInvisible;
                if (culled)
                {
                    Culled++;
                    if (!s.ShowCulled)
                        continue;
                }

                Vector3 pos = module.Position;
                if (!IsSane(pos))
                    continue;
                float distance = Vector3.Distance(eye, pos);
                if (!(distance <= s.PlayerMaxDistance))
                    continue;

                var t = new Target { Color = PickColor(role, team, s, culled) };

                CharacterController cc = module.CharControllerSet ? module.CharController : null;
                if (s.PlayerBox && cc != null)
                    t.HasBox = TryBuildBox(cam, cc, out t.Box);

                if (t.HasBox)
                {
                    if (!ScreenMath.IntersectsScreen(t.Box.x, t.Box.y, t.Box.width, t.Box.height, screenW, screenH))
                        continue;
                    t.Anchor = new Vector2(t.Box.center.x, t.Box.yMax);
                }
                else
                {
                    if (!ViewCamera.WorldToGui(cam, pos, out t.Anchor))
                        continue;
                    if (!ScreenMath.IntersectsScreen(t.Anchor.x, t.Anchor.y, 0f, 0f, screenW, screenH))
                        continue;
                }

                if (s.PlayerHeadDot)
                    t.HasHead = TryGetHead(cam, module, out t.Head);

                if (s.PlayerName)
                    t.Name = Clip(hub.nicknameSync != null ? hub.nicknameSync.DisplayName : null);

                string info = null;
                if (s.PlayerRole)
                    info = role.RoleName;
                if (s.PlayerDistance)
                    info = (info == null ? "" : info + "  ") + Mathf.RoundToInt(distance) + "m";
                if (culled)
                    info = (info == null ? "" : info + "  ") + "(last known)";
                t.Info = info;

                if (s.PlayerHealth && !localAlive)
                    t.Health = ReadHealth(hub);

                _targets.Add(t);
                Drawn++;
            }
        }

        private static bool TryBuildBox(Camera cam, CharacterController cc, out Rect box)
        {
            box = default;
            Transform tr = cc.transform;
            Vector3 scale = tr.lossyScale;
            float half = cc.height * 0.5f * Mathf.Abs(scale.y);
            float radius = cc.radius * Mathf.Max(Mathf.Abs(scale.x), Mathf.Abs(scale.z));
            if (!(half > 0.05f && half < 10f) || !(radius > 0.01f && radius < 10f))
                return false;

            // FpcStateProcessor changes height/center while crouching, so read them live every frame.
            Vector3 center = tr.TransformPoint(cc.center);
            if (!IsSane(center))
                return false;

            if (!ViewCamera.WorldToGui(cam, center + Vector3.up * half, out Vector2 top) ||
                !ViewCamera.WorldToGui(cam, center - Vector3.up * half, out Vector2 bottom))
                return false;

            if (!ScreenMath.BoxFromVertical(top.x, top.y, bottom.x, bottom.y, radius / half,
                                            out float x, out float y, out float w, out float h))
                return false;

            box = new Rect(x, y, w, h);
            return true;
        }

        private static bool TryGetHead(Camera cam, FirstPersonMovementModule module, out Vector2 head)
        {
            head = default;
            CharacterModel model = module.CharacterModelInstance;
            if (model == null)
                return false;
            HitboxIdentity[] hitboxes = model.Hitboxes;
            if (hitboxes == null)
                return false;

            for (int i = 0; i < hitboxes.Length && i < 64; i++)
            {
                HitboxIdentity hb = hitboxes[i];
                if (hb == null || hb.HitboxType != HitboxType.Headshot)
                    continue;
                Vector3 p = hb.CenterOfMass;
                return IsSane(p) && ViewCamera.WorldToGui(cam, p, out head);
            }
            return false;
        }

        private static string ReadHealth(ReferenceHub hub)
        {
            PlayerStats stats = hub.playerStats;
            if (stats == null)
                return null;
            try
            {
                HealthStat hp = stats.GetModule<HealthStat>();
                if (hp == null)
                    return null;
                string text = "HP " + Mathf.CeilToInt(hp.CurValue) + "/" + Mathf.CeilToInt(hp.MaxValue);
                HumeShieldStat hs = stats.GetModule<HumeShieldStat>();
                if (hs != null && hs.CurValue > 0f)
                    text += "  HS " + Mathf.CeilToInt(hs.CurValue);
                return text;
            }
            catch (KeyNotFoundException)
            {
                return null; // stat modules not registered yet on this hub
            }
        }

        private static Color PickColor(PlayerRoleBase role, Team team, Settings s, bool culled)
        {
            Color c = s.UseRoleColor ? role.RoleColor : TeamColor(team);
            c.a = culled ? 0.4f : 1f;
            return c;
        }

        private static Color TeamColor(Team team)
        {
            switch (team)
            {
                case Team.SCPs: return new Color(0.93f, 0.13f, 0.13f);
                case Team.FoundationForces: return new Color(0.2f, 0.55f, 1f);
                case Team.ChaosInsurgency: return new Color(0.1f, 0.75f, 0.25f);
                case Team.Scientists: return new Color(1f, 0.95f, 0.5f);
                case Team.ClassD: return new Color(1f, 0.55f, 0.1f);
                case Team.Flamingos: return new Color(1f, 0.45f, 0.75f);
                default: return Color.white;
            }
        }

        private static bool IsSane(Vector3 v)
        {
            return ScreenMath.IsFinite(v.x) && ScreenMath.IsFinite(v.y) && ScreenMath.IsFinite(v.z) &&
                   Mathf.Abs(v.x) < MaxCoordinate && Mathf.Abs(v.y) < MaxCoordinate && Mathf.Abs(v.z) < MaxCoordinate;
        }

        private static string Clip(string s)
        {
            if (string.IsNullOrEmpty(s))
                return "?";
            return s.Length > MaxNameLength ? s.Substring(0, MaxNameLength) : s;
        }
    }
}
