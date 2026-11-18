using UnityEngine;

namespace SLMenu.Render
{
    /// <summary>IMGUI drawing primitives. Call only from OnGUI.</summary>
    internal static class Draw
    {
        private static Texture2D _white;
        private static GUIStyle _label;
        private static readonly GUIContent Content = new GUIContent();
        private static readonly Color Shadow = new Color(0f, 0f, 0f, 0.85f);

        private static Texture2D White
        {
            get
            {
                if (_white == null)
                {
                    _white = new Texture2D(1, 1, TextureFormat.RGBA32, false) { hideFlags = HideFlags.HideAndDontSave };
                    _white.SetPixel(0, 0, Color.white);
                    _white.Apply();
                }
                return _white;
            }
        }

        private static GUIStyle Label
        {
            get
            {
                if (_label == null)
                {
                    _label = new GUIStyle(GUI.skin.label)
                    {
                        fontSize = 11,
                        alignment = TextAnchor.UpperCenter,
                        wordWrap = false,
                        richText = false,
                        clipping = TextClipping.Overflow,
                    };
                    _label.normal.textColor = Color.white; // GUI.color tints it to the requested colour
                    _label.padding = new RectOffset(0, 0, 0, 0);
                    _label.margin = new RectOffset(0, 0, 0, 0);
                }
                return _label;
            }
        }

        public static void FillRect(Rect r, Color c)
        {
            Color prev = GUI.color;
            GUI.color = c;
            GUI.DrawTexture(r, White);
            GUI.color = prev;
        }

        public static void BoxOutline(Rect r, Color c, float thickness)
        {
            // dark outline around the coloured frame keeps it readable on bright backgrounds
            Frame(new Rect(r.x - 1f, r.y - 1f, r.width + 2f, r.height + 2f), Shadow, thickness + 2f);
            Frame(r, c, thickness);
        }

        private static void Frame(Rect r, Color c, float t)
        {
            FillRect(new Rect(r.x, r.y, r.width, t), c);
            FillRect(new Rect(r.x, r.yMax - t, r.width, t), c);
            FillRect(new Rect(r.x, r.y, t, r.height), c);
            FillRect(new Rect(r.xMax - t, r.y, t, r.height), c);
        }

        public static void Dot(Vector2 center, float size, Color c)
        {
            FillRect(new Rect(center.x - size * 0.5f - 1f, center.y - size * 0.5f - 1f, size + 2f, size + 2f), Shadow);
            FillRect(new Rect(center.x - size * 0.5f, center.y - size * 0.5f, size, size), c);
        }

        public static void Line(Vector2 a, Vector2 b, Color c, float thickness)
        {
            Vector2 d = b - a;
            float length = d.magnitude;
            if (length < 0.5f || !ScreenMath.IsFinite(length))
                return;

            float angle = Mathf.Atan2(d.y, d.x) * Mathf.Rad2Deg;
            Matrix4x4 saved = GUI.matrix;
            GUIUtility.RotateAroundPivot(angle, a);
            FillRect(new Rect(a.x, a.y - thickness * 0.5f, length, thickness), c);
            GUI.matrix = saved;
        }

        /// <summary>Draws text horizontally centred on x, with its top edge at y. Returns the text height.</summary>
        public static float Text(float x, float y, string text, Color c)
        {
            if (string.IsNullOrEmpty(text))
                return 0f;

            Content.text = text;
            Vector2 size = Label.CalcSize(Content);
            var r = new Rect(x - size.x * 0.5f, y, size.x, size.y);

            Color prev = GUI.color;
            GUI.color = Shadow;
            GUI.Label(new Rect(r.x + 1f, r.y + 1f, r.width, r.height), Content, Label);
            GUI.color = c;
            GUI.Label(r, Content, Label);
            GUI.color = prev;
            return size.y;
        }

        public static void Release()
        {
            if (_white != null)
            {
                Object.Destroy(_white);
                _white = null;
            }
            _label = null;
        }
    }
}
