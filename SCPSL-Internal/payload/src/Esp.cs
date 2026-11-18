using PlayerRoles;
using UnityEngine;

namespace SLInternal
{
    internal sealed class Esp
    {
        private readonly Settings _s;
        private readonly WorldCache _world;

        public Esp(Settings s, WorldCache world)
        {
            _s = s;
            _world = world;
        }

        public static Color TeamColor(Team team)
        {
            switch (team)
            {
                case Team.SCPs: return new Color(1f, 0.2f, 0.2f);
                case Team.FoundationForces: return new Color(0.25f, 0.55f, 1f);
                case Team.Scientists: return new Color(1f, 0.95f, 0.35f);
                case Team.ChaosInsurgency: return new Color(0.2f, 0.85f, 0.3f);
                case Team.ClassD: return new Color(1f, 0.6f, 0.15f);
                case Team.Flamingos: return new Color(1f, 0.45f, 0.75f);
                case Team.OtherAlive: return new Color(0.85f, 0.4f, 1f);
                default: return Color.white;
            }
        }

        public void Draw(Camera cam)
        {
            if (!_s.EspEnabled || cam == null || _world.Local == null)
                return;

            foreach (PlayerInfo p in _world.Players)
            {
                if (!_s.EspTeammates && !p.Enemy)
                    continue;
                if (p.Distance > _s.EspMaxDistance)
                    continue;
                if (!ViewCamera.BoundsToGui(cam, p.Box, out Rect box))
                    continue;

                Color col = TeamColor(p.Team);
                if (!p.Enemy)
                    col.a = 0.6f;

                if (_s.EspBox)
                    SLInternal.Draw.Box(box, col);

                if (_s.EspHealth && p.HealthValid)
                {
                    float frac = Mathf.Clamp01(p.Health / p.MaxHealth);
                    var bar = new Rect(box.xMin - 5f, box.yMin, 3f, box.height);
                    SLInternal.Draw.Rect(new Rect(bar.x - 1f, bar.y - 1f, bar.width + 2f, bar.height + 2f), new Color(0f, 0f, 0f, 0.7f));
                    float h = bar.height * frac;
                    SLInternal.Draw.Rect(new Rect(bar.x, bar.yMax - h, bar.width, h), Color.Lerp(Color.red, Color.green, frac));
                }

                float top = box.yMin - 2f;
                if (_s.EspName)
                {
                    top -= 14f;
                    SLInternal.Draw.Text(new Vector2(box.center.x, top), p.Name, col);
                }

                float bottom = box.yMax + 2f;
                if (_s.EspRole)
                {
                    SLInternal.Draw.Text(new Vector2(box.center.x, bottom), _world.RoleName(p.Role), col);
                    bottom += 14f;
                }
                if (_s.EspDistance || (_s.EspHealth && p.HealthValid))
                {
                    string line = _s.EspDistance ? $"{p.Distance:0}m" : string.Empty;
                    if (_s.EspHealth && p.HealthValid)
                        line += (line.Length > 0 ? "  " : string.Empty) + $"{p.Health:0}hp";
                    SLInternal.Draw.Text(new Vector2(box.center.x, bottom), line, Color.white);
                }

                if (_s.EspSnaplines && p.Enemy)
                    SLInternal.Draw.Line(new Vector2(Screen.width * 0.5f, Screen.height), new Vector2(box.center.x, box.yMax), col);
            }
        }
    }
}
