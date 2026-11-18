using System;
using InventorySystem.Items;
using SLMenu.Esp;
using SLMenu.Render;
using UnityEngine;

namespace SLMenu
{
    /// <summary>Owns the menu window and both ESP features. Everything is collected and drawn in OnGUI Repaint,
    /// which runs after the frame's LateUpdate, so positions and camera match the rendered frame.</summary>
    internal sealed class MenuBehaviour : MonoBehaviour
    {
        private const int WindowId = 0x534C4D;

        private readonly Settings _s = new Settings();
        private GameBuild.Report _report;
        private PlayerEsp _playerEsp;
        private PickupTracker _pickups;
        private ItemEsp _itemEsp;
        private CursorUnlock _cursor;

        private string _playerError;
        private string _itemError;
        private string _cursorError;

        private bool _open = true;
        private int _tab;
        private Rect _window = new Rect(60f, 60f, 380f, 10f);
        private Vector2 _scroll;

        private static readonly string[] Tabs = { "Players", "Items", "Info" };

        private void Awake()
        {
            _report = GameBuild.Validate();
            LogReport();

            _playerEsp = new PlayerEsp();
            if (_report.ItemEspAvailable)
                StartItemEsp();
            if (_report.CursorOverrideAvailable)
                AttachCursorUnlock();
        }

        private void StartItemEsp()
        {
            try
            {
                _pickups = new PickupTracker();
                _pickups.Start();
                _itemEsp = new ItemEsp(_pickups);
                _itemError = null;
            }
            catch (Exception e)
            {
                _itemError = Describe(e);
                _pickups?.Stop();
                _pickups = null;
                _itemEsp = null;
            }
        }

        private void AttachCursorUnlock()
        {
            try
            {
                _cursor = gameObject.AddComponent<CursorUnlock>();
            }
            catch (Exception e)
            {
                _cursorError = Describe(e);
            }
        }

        private void OnDestroy()
        {
            try { _pickups?.Stop(); } catch (Exception e) { Debug.LogError("[SLMenu] " + e); }
            if (_cursor != null)
                Destroy(_cursor);
            Draw.Release();
        }

        private void Update()
        {
            if (Input.GetKeyDown(_s.MenuKey))
                _open = !_open;
            if (_cursor != null)
                _cursor.Active = _open && Application.isFocused;
        }

        private void OnGUI()
        {
            if (Event.current.type == EventType.Repaint)
                RenderEsp();

            if (_open)
            {
                GUI.color = Color.white;
                _window = GUILayout.Window(WindowId, _window, DrawWindow, "SLMenu  [" + _s.MenuKey + "]",
                                           GUILayout.Width(380f));
            }
        }

        private void RenderEsp()
        {
            Camera cam = ViewCamera.Resolve();
            if (cam == null)
                return;

            if (_s.PlayerEsp && _report.PlayerEspAvailable && _playerError == null)
            {
                try { _playerEsp.Render(cam, _s); }
                catch (Exception e) { _playerError = Describe(e); Debug.LogError("[SLMenu] Player ESP: " + e); }
            }

            if (_s.ItemEsp && _itemEsp != null && _itemError == null)
            {
                try { _itemEsp.Render(cam, _s); }
                catch (Exception e) { _itemError = Describe(e); Debug.LogError("[SLMenu] Item ESP: " + e); }
            }
        }

        private void DrawWindow(int id)
        {
            _tab = GUILayout.Toolbar(_tab, Tabs);
            GUILayout.Space(4f);
            switch (_tab)
            {
                case 0: DrawPlayersTab(); break;
                case 1: DrawItemsTab(); break;
                default: DrawInfoTab(); break;
            }
            GUI.DragWindow(new Rect(0f, 0f, 10000f, 20f));
        }

        private void DrawPlayersTab()
        {
            if (!_report.PlayerEspAvailable)
            {
                GUILayout.Label("Player ESP unavailable on this build. See Info.");
                return;
            }
            ErrorRow(ref _playerError);

            _s.PlayerEsp = GUILayout.Toggle(_s.PlayerEsp, "Enabled");
            _s.PlayerBox = GUILayout.Toggle(_s.PlayerBox, "Box");
            _s.PlayerHeadDot = GUILayout.Toggle(_s.PlayerHeadDot, "Head dot");
            _s.PlayerName = GUILayout.Toggle(_s.PlayerName, "Name");
            _s.PlayerRole = GUILayout.Toggle(_s.PlayerRole, "Role");
            _s.PlayerDistance = GUILayout.Toggle(_s.PlayerDistance, "Distance");
            _s.PlayerHealth = GUILayout.Toggle(_s.PlayerHealth, "Health (only while you are dead / spectating)");
            _s.PlayerSnaplines = GUILayout.Toggle(_s.PlayerSnaplines, "Snaplines");
            _s.ShowTeammates = GUILayout.Toggle(_s.ShowTeammates, "Show teammates");
            _s.ShowCulled = GUILayout.Toggle(_s.ShowCulled, "Show last known position of server-culled players");
            _s.UseRoleColor = GUILayout.Toggle(_s.UseRoleColor, "Game role colours (off: team colours)");
            _s.PlayerMaxDistance = Slider("Max distance", _s.PlayerMaxDistance, 10f, 500f);
            GUILayout.Label("Drawn " + _playerEsp.Drawn + ", position withheld by server: " + _playerEsp.Culled);
        }

        private void DrawItemsTab()
        {
            if (!_report.ItemEspAvailable)
            {
                GUILayout.Label("Item ESP unavailable on this build. See Info.");
                return;
            }
            if (_itemEsp == null && _itemError == null)
                _itemError = "Item tracker not started.";
            if (ErrorRow(ref _itemError) && _itemEsp == null)
                StartItemEsp();

            _s.ItemEsp = GUILayout.Toggle(_s.ItemEsp, "Enabled");
            _s.ItemDistance = GUILayout.Toggle(_s.ItemDistance, "Distance");
            _s.ItemShowLocked = GUILayout.Toggle(_s.ItemShowLocked, "Show locked pickups");
            _s.ItemMaxDistance = Slider("Max distance", _s.ItemMaxDistance, 5f, 300f);

            GUILayout.Label("Categories:");
            foreach (ItemCategory c in Enum.GetValues(typeof(ItemCategory)))
            {
                byte i = (byte)c;
                if (i >= _s.ItemCategoryEnabled.Length)
                    continue;
                Color prev = GUI.contentColor;
                GUI.contentColor = ItemEsp.CategoryColor(c);
                _s.ItemCategoryEnabled[i] = GUILayout.Toggle(_s.ItemCategoryEnabled[i], c.ToString());
                GUI.contentColor = prev;
            }

            GUILayout.Label("Tracked pickups " + (_pickups != null ? _pickups.Count : 0) +
                            ", drawn " + (_itemEsp != null ? _itemEsp.Drawn : 0));
        }

        private void DrawInfoTab()
        {
            _scroll = GUILayout.BeginScrollView(_scroll, GUILayout.Height(300f));
            GUILayout.Label("Game version: " + _report.GameVersion + (_report.VersionMatches ? "  (matches reference)" : "  (NOT the reference build)"));
            GUILayout.Label("Written against: " + GameBuild.RefSource);
            GUILayout.Label("Runtime: " + (_report.IsMono ? "Mono" : "not Mono"));
            GUILayout.Label("Game assembly: " + _report.AssemblyName);
            GUILayout.Label(_report.AssemblyLocation);
            GUILayout.Label("Camera: " + ViewCamera.Source);
            GUILayout.Label("Camera offset from MainCameraController.LastPosition: " +
                            (ViewCamera.OffsetFromGameCamera < 0f ? "n/a" : ViewCamera.OffsetFromGameCamera.ToString("0.000") + " m"));
            GUILayout.Label("Screen: " + Screen.width + "x" + Screen.height);
            GUILayout.Label("Cursor unlock: " + (_cursor != null ? "active" : _cursorError ?? "unavailable"));

            if (_report.Problems.Count == 0)
            {
                GUILayout.Label("Validation: all required types and members found.");
            }
            else
            {
                GUILayout.Label("Validation problems:");
                foreach (string p in _report.Problems)
                    GUILayout.Label("- " + p);
            }
            GUILayout.EndScrollView();

            GUILayout.Space(6f);
            if (GUILayout.Button("Unload SLMenu"))
                Loader.Unload();
        }

        /// <summary>Shows a feature error with a Retry button. Returns true when Retry was pressed (error cleared).</summary>
        private static bool ErrorRow(ref string error)
        {
            if (error == null)
                return false;
            Color prev = GUI.contentColor;
            GUI.contentColor = new Color(1f, 0.45f, 0.4f);
            GUILayout.Label("Stopped after error: " + error);
            GUI.contentColor = prev;
            if (!GUILayout.Button("Retry"))
                return false;
            error = null;
            return true;
        }

        private static float Slider(string label, float value, float min, float max)
        {
            GUILayout.Label(label + ": " + Mathf.RoundToInt(value) + " m");
            return GUILayout.HorizontalSlider(value, min, max);
        }

        private static string Describe(Exception e)
        {
            return e.GetType().Name + ": " + e.Message;
        }

        private void LogReport()
        {
            Debug.Log("[SLMenu] Game " + _report.GameVersion + ", reference " + GameBuild.RefSource +
                      ", Mono=" + _report.IsMono + ", assembly " + _report.AssemblyName + " @ " + _report.AssemblyLocation);
            foreach (string p in _report.Problems)
                Debug.LogWarning("[SLMenu] " + p);
        }
    }
}
