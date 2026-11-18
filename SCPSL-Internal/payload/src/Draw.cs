using UnityEngine;

namespace SLInternal
{
    // IMGUI primitives. Call only from OnGUI.
    internal static class Draw
    {
        private static Texture2D _white;
        private static GUIStyle _text;
        private static readonly GUIContent Content = new GUIContent();
        private static readonly Color Shadow = new Color(0f, 0f, 0f, 0.85f);

        private static void Ensure()
        {
            if (_white == null)
            {
                _white = new Texture2D(1, 1, TextureFormat.RGBA32, false) { hideFlags = HideFlags.HideAndDontSave };
                _white.SetPixel(0, 0, Color.white);
                _white.Apply();
            }
            if (_text == null)
            {
                _text = new GUIStyle(GUI.skin.label) { fontSize = 12, richText = false, wordWrap = false, clipping = TextClipping.Overflow };
                _text.padding = new RectOffset(0, 0, 0, 0);
                _text.margin = new RectOffset(0, 0, 0, 0);
            }
        }

        public static void Dispose()
        {
            if (_white != null)
                Object.Destroy(_white);
            _white = null;
            _text = null;
        }

        public static void Rect(Rect r, Color c)
        {
            Ensure();
            Color old = GUI.color;
            GUI.color = c;
            GUI.DrawTexture(r, _white);
            GUI.color = old;
        }

        public static void Outline(Rect r, Color c, float t = 1f)
        {
            Rect(new Rect(r.xMin, r.yMin, r.width, t), c);
            Rect(new Rect(r.xMin, r.yMax - t, r.width, t), c);
            Rect(new Rect(r.xMin, r.yMin, t, r.height), c);
            Rect(new Rect(r.xMax - t, r.yMin, t, r.height), c);
        }

        public static void Box(Rect r, Color c)
        {
            Outline(new Rect(r.x - 1f, r.y - 1f, r.width + 2f, r.height + 2f), Shadow);
            Outline(new Rect(r.x + 1f, r.y + 1f, r.width - 2f, r.height - 2f), Shadow);
            Outline(r, c);
        }

        public static void Line(Vector2 a, Vector2 b, Color c, float t = 1f)
        {
            Ensure();
            Vector2 d = b - a;
            float len = d.magnitude;
            if (len < 0.5f)
                return;
            Matrix4x4 m = GUI.matrix;
            Color old = GUI.color;
            GUI.color = c;
            GUIUtility.RotateAroundPivot(Mathf.Atan2(d.y, d.x) * Mathf.Rad2Deg, a);
            GUI.DrawTexture(new Rect(a.x, a.y - t * 0.5f, len, t), _white);
            GUI.matrix = m;
            GUI.color = old;
        }

        public static void Circle(Vector2 centre, float radius, Color c, int segments = 48)
        {
            if (radius < 1f)
                return;
            float step = Mathf.PI * 2f / segments;
            Vector2 prev = centre + new Vector2(radius, 0f);
            for (int i = 1; i <= segments; i++)
            {
                Vector2 next = centre + new Vector2(Mathf.Cos(step * i) * radius, Mathf.Sin(step * i) * radius);
                Line(prev, next, c);
                prev = next;
            }
        }

        public static Vector2 TextSize(string s)
        {
            Ensure();
            Content.text = s;
            return _text.CalcSize(Content);
        }

        // anchorX: 0 left, 0.5 centred, 1 right; y is the top edge.
        public static void Text(Vector2 pos, string s, Color c, float anchorX = 0.5f)
        {
            Ensure();
            Content.text = s;
            Vector2 size = _text.CalcSize(Content);
            var r = new Rect(pos.x - size.x * anchorX, pos.y, size.x, size.y);
            Color old = GUI.contentColor;
            GUI.contentColor = Shadow;
            GUI.Label(new Rect(r.x + 1f, r.y + 1f, r.width, r.height), Content, _text);
            GUI.contentColor = c;
            GUI.Label(r, Content, _text);
            GUI.contentColor = old;
        }
    }
}
