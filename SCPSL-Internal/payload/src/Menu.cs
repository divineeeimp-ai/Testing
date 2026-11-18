using System;
using UnityEngine;

namespace SLInternal
{
    internal sealed class Menu
    {
        private static readonly string[] Tabs = { "Visuals", "Items", "Aim", "Auto shoot", "Settings" };
        private const int WindowId = 0x51A7;

        private readonly Main _main;
        private readonly Settings _s;
        private Rect _window = new Rect(60f, 60f, 400f, 500f);
        private Vector2 _scroll;
        private int _tab;
        private string _saveStatus = string.Empty;

        // Key binding: the field being bound, and whether the click that started it was released.
        private Action<int> _bindTarget;
        private string _bindName;
        private bool _bindArmed;

        public bool Binding => _bindTarget != null;

        public Menu(Main main, Settings s)
        {
            _main = main;
            _s = s;
        }

        // Main thread, every frame while the menu is open.
        public void UpdateBinder()
        {
            if (_bindTarget == null)
                return;
            int vk = Win32.FirstKeyDown();
            if (!_bindArmed)
            {
                _bindArmed = vk == 0;
                return;
            }
            if (vk == 0)
                return;
            if (vk != 0x1B) // Esc cancels, Backspace clears
                _bindTarget(vk == 0x08 ? 0 : vk);
            _bindTarget = null;
        }

        public void Draw()
        {
            _window = GUI.Window(WindowId, _window, DrawWindow, "SLInternal  [" + Win32.KeyName(_s.MenuKey) + "]");
            _window.x = Mathf.Clamp(_window.x, 0f, Mathf.Max(0f, Screen.width - _window.width));
            _window.y = Mathf.Clamp(_window.y, 0f, Mathf.Max(0f, Screen.height - _window.height));
        }

        private void DrawWindow(int id)
        {
            _tab = GUILayout.Toolbar(_tab, Tabs);
            _scroll = GUILayout.BeginScrollView(_scroll);
            switch (_tab)
            {
                case 0: Visuals(); break;
                case 1: Items(); break;
                case 2: Aim(); break;
                case 3: AutoShoot(); break;
                default: SettingsTab(); break;
            }
            GUILayout.EndScrollView();
            GUI.DragWindow(new Rect(0f, 0f, 10000f, 20f));
        }

        private void Visuals()
        {
            Toggle(ref _s.EspEnabled, "Player ESP");
            Toggle(ref _s.EspBox, "Box");
            Toggle(ref _s.EspName, "Name");
            Toggle(ref _s.EspHealth, "Health (only if the server syncs it)");
            Toggle(ref _s.EspRole, "Role");
            Toggle(ref _s.EspDistance, "Distance");
            Toggle(ref _s.EspSnaplines, "Snaplines (enemies)");
            Toggle(ref _s.EspTeammates, "Show teammates");
            Slider(ref _s.EspMaxDistance, 10f, 500f, "Max distance", "0 m");
        }

        private void Items()
        {
            Toggle(ref _s.ItemEspEnabled, "Item ESP");
            Toggle(ref _s.ItemKeycards, "Keycards");
            Toggle(ref _s.ItemFirearms, "Firearms");
            Toggle(ref _s.ItemMedical, "Medical");
            Toggle(ref _s.ItemScp, "SCP items");
            Toggle(ref _s.ItemGrenades, "Grenades");
            Toggle(ref _s.ItemArmor, "Armor");
            Toggle(ref _s.ItemAmmo, "Ammo");
            Toggle(ref _s.ItemOther, "Other");
            Slider(ref _s.ItemMaxDistance, 5f, 200f, "Max distance", "0 m");
            GUILayout.Label($"Tracked pickups: {_main.PickupCount}");
        }

        private void Aim()
        {
            if (!_s.Calibrated)
                GUILayout.Label("Mouse not calibrated: aim assist stays off until you run Settings > Calibrate.");
            Toggle(ref _s.AimEnabled, "Aim assist");
            Key("Hold key", _s.AimKey, v => _s.AimKey = v);
            Slider(ref _s.AimFov, 0.5f, 30f, "FOV (half-angle)", "0.0°");
            Slider(ref _s.AimSpeed, 1f, 30f, "Speed", "0.0 /s");
            Slider(ref _s.AimMaxDegPerSec, 10f, 720f, "Max turn rate", "0 °/s");
            Slider(ref _s.AimDeadzone, 0f, 1f, "Deadzone", "0.00°");
            Slider(ref _s.AimHeight, 0f, 1f, "Aim point (0 body .. 1 eye)", "0.00");
            Slider(ref _s.AimMaxDistance, 5f, 300f, "Max distance", "0 m");
            Toggle(ref _s.AimVisibleOnly, "Visible targets only");
            Toggle(ref _s.AimStickyTarget, "Stick to current target");
            Toggle(ref _s.AimTeammates, "Include teammates");
            Toggle(ref _s.AimDrawFov, "Draw FOV circle");
        }

        private void AutoShoot()
        {
            Toggle(ref _s.TrigEnabled, "Auto shoot");
            Toggle(ref _s.TrigRequireKey, "Only while key held");
            Key("Key", _s.TrigKey, v => _s.TrigKey = v);
            Slider(ref _s.TrigDelayMs, 0f, 500f, "Reaction delay", "0 ms");
            Toggle(ref _s.TrigHoldWhileOnTarget, "Hold fire while on target (automatic weapons)");
            if (!_s.TrigHoldWhileOnTarget)
            {
                Slider(ref _s.TrigHoldMs, 10f, 300f, "Click length", "0 ms");
                Slider(ref _s.TrigIntervalMs, 0f, 1000f, "Pause between clicks", "0 ms");
            }
            Slider(ref _s.TrigMaxDistance, 5f, 300f, "Max distance", "0 m");
            Toggle(ref _s.TrigTeammates, "Include teammates");
            GUILayout.Label(_main.TriggerOnTarget ? "Crosshair: ON TARGET" : "Crosshair: no target");
        }

        private void SettingsTab()
        {
            GUILayout.Label("Mouse calibration");
            GUILayout.Label(_main.CalibrationMessage);
            if (GUILayout.Button("Calibrate (closes menu, look at the horizon, hands off the mouse)"))
                _main.StartCalibration();
            if (_s.Calibrated && GUILayout.Button("Forget calibration"))
                _s.DegPerCountX = _s.DegPerCountY = float.NaN;

            GUILayout.Space(8f);
            Key("Menu key", _s.MenuKey, v => _s.MenuKey = v == 0 ? 0x2D : v);
            Key("Unload key", _s.PanicKey, v => _s.PanicKey = v);

            GUILayout.Space(8f);
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Save config"))
                _saveStatus = _s.Save() ? "saved to " + Settings.FilePath : "save failed (see log)";
            if (GUILayout.Button("Unload"))
                Loader.Unload();
            GUILayout.EndHorizontal();
            if (_saveStatus.Length > 0)
                GUILayout.Label(_saveStatus);

            GUILayout.Space(8f);
            GUILayout.Label("Status");
            GUILayout.Label("Camera: " + _main.CameraDescription);
            GUILayout.Label("Local player: " + _main.LocalDescription);
            GUILayout.Label($"Players cached: {_main.PlayerCount}");
            foreach (string line in GameApi.Status)
                GUILayout.Label(line);
        }

        private static void Toggle(ref bool value, string label) => value = GUILayout.Toggle(value, " " + label);

        private static void Slider(ref float value, float min, float max, string label, string format)
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label(label, GUILayout.Width(190f));
            value = GUILayout.HorizontalSlider(value, min, max, GUILayout.ExpandWidth(true));
            GUILayout.Label(value.ToString(format), GUILayout.Width(70f));
            GUILayout.EndHorizontal();
        }

        private void Key(string label, int current, Action<int> set)
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label(label, GUILayout.Width(190f));
            bool waiting = _bindTarget != null && _bindName == label;
            if (GUILayout.Button(waiting ? "press a key (Esc cancel, Backspace none)" : Win32.KeyName(current)))
            {
                _bindTarget = set;
                _bindName = label;
                _bindArmed = false;
            }
            GUILayout.EndHorizontal();
        }
    }
}
