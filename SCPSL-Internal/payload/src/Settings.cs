using System;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;

namespace SLInternal
{
    // Every public field is saved to SLInternal.cfg as name=value (invariant culture).
    internal sealed class Settings
    {
        // Player ESP
        public bool EspEnabled = true;
        public bool EspBox = true;
        public bool EspName = true;
        public bool EspHealth = true;
        public bool EspRole = true;
        public bool EspDistance = true;
        public bool EspSnaplines = false;
        public bool EspTeammates = true;
        public float EspMaxDistance = 150f;

        // Item ESP
        public bool ItemEspEnabled = true;
        public bool ItemKeycards = true;
        public bool ItemFirearms = true;
        public bool ItemMedical = true;
        public bool ItemScp = true;
        public bool ItemGrenades = true;
        public bool ItemArmor = false;
        public bool ItemAmmo = false;
        public bool ItemOther = false;
        public float ItemMaxDistance = 40f;

        // Aim assist (legit). Angles in degrees, AimFov is the half-angle of the cone around the crosshair.
        public bool AimEnabled = false;
        public int AimKey = 0x02; // Mouse2 (aim down sights)
        public float AimFov = 5f;
        public float AimSpeed = 8f;          // 1/s: fraction of the error removed per second (exponential)
        public float AimMaxDegPerSec = 120f; // hard cap on correction speed
        public float AimDeadzone = 0.1f;
        public float AimHeight = 1f;         // 0 = body centre (FpcModule.Position), 1 = eye (PlayerCameraReference)
        public float AimMaxDistance = 100f;
        public bool AimVisibleOnly = true;
        public bool AimStickyTarget = true;
        public bool AimTeammates = false;
        public bool AimDrawFov = true;

        // Mouse calibration, measured in game by the Calibrate button. NaN means not measured.
        public float DegPerCountX = float.NaN;
        public float DegPerCountY = float.NaN;

        // Auto shoot
        public bool TrigEnabled = false;
        public bool TrigRequireKey = true;
        public int TrigKey = 0x02; // Mouse2
        public float TrigDelayMs = 60f;
        public float TrigHoldMs = 40f;
        public float TrigIntervalMs = 120f;
        public bool TrigHoldWhileOnTarget = false;
        public float TrigMaxDistance = 100f;
        public bool TrigTeammates = false;

        // Hotkeys
        public int MenuKey = 0x2D;  // Insert
        public int PanicKey = 0x23; // End: unload

        public bool Calibrated =>
            AimMath.IsFinite(DegPerCountX) && AimMath.IsFinite(DegPerCountY) && DegPerCountX != 0f && DegPerCountY != 0f;

        public static string FilePath => Log.BaseDir == null ? null : Path.Combine(Log.BaseDir, "SLInternal.cfg");

        public bool Save()
        {
            string path = FilePath;
            if (path == null)
                return false;
            var sb = new StringBuilder();
            foreach (FieldInfo f in typeof(Settings).GetFields(BindingFlags.Public | BindingFlags.Instance))
                sb.Append(f.Name).Append('=').Append(Convert.ToString(f.GetValue(this), CultureInfo.InvariantCulture)).Append('\n');
            try
            {
                File.WriteAllText(path, sb.ToString());
                return true;
            }
            catch (Exception e)
            {
                Log.Error("saving settings: " + e.Message);
                return false;
            }
        }

        public static Settings Load()
        {
            var s = new Settings();
            string path = FilePath;
            if (path == null || !File.Exists(path))
                return s;
            try
            {
                foreach (string raw in File.ReadAllLines(path))
                {
                    int eq = raw.IndexOf('=');
                    if (eq <= 0)
                        continue;
                    FieldInfo f = typeof(Settings).GetField(raw.Substring(0, eq).Trim(), BindingFlags.Public | BindingFlags.Instance);
                    if (f == null)
                        continue;
                    string v = raw.Substring(eq + 1).Trim();
                    if (f.FieldType == typeof(bool) && bool.TryParse(v, out bool b)) f.SetValue(s, b);
                    else if (f.FieldType == typeof(int) && int.TryParse(v, NumberStyles.Integer, CultureInfo.InvariantCulture, out int i)) f.SetValue(s, i);
                    else if (f.FieldType == typeof(float) && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out float fl)) f.SetValue(s, fl);
                }
                Log.Info("settings loaded from " + path);
            }
            catch (Exception e)
            {
                Log.Error("loading settings: " + e.Message);
            }
            return s;
        }
    }
}
