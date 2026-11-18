using System;
using System.Collections.Generic;
using System.Reflection;

namespace SLMenu
{
    /// <summary>
    /// Which game build the code was written against, and a runtime check of the loaded game.
    /// Every class and member name used by SLMenu was read from Assembly-CSharp of
    /// SCP: Secret Laboratory 14.2.8 (Release). Source artifact: Assembly-CSharp-Publicized.dll
    /// from NuGet package ExMod.Exiled 9.15.2 (the dedicated-server build of the same codebase).
    /// The client build on disk was not available when this was written.
    /// SLMenu uses no raw offsets: all access goes through managed type and member names.
    /// </summary>
    internal static class GameBuild
    {
        public const byte RefMajor = 14;
        public const byte RefMinor = 2;
        public const byte RefRevision = 8;
        public const string RefSource = "Assembly-CSharp 14.2.8 (from ExMod.Exiled 9.15.2 NuGet, server build)";

        internal sealed class Report
        {
            public bool IsMono;
            public string AssemblyName = "?";
            public string AssemblyLocation = "?";
            public string GameVersion = "unknown";
            public bool VersionMatches;
            public bool PlayerEspAvailable;
            public bool ItemEspAvailable;
            public bool CursorOverrideAvailable;
            public readonly List<string> Problems = new List<string>();
        }

        private const BindingFlags AnyMember =
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;

        // (type full name, member name). Checked by name before any typed code runs, so a renamed member
        // turns its feature off with a readable message instead of a MissingMemberException every frame.
        private static readonly string[][] PlayerMembers =
        {
            new[] { "ReferenceHub", "AllHubs" },
            new[] { "ReferenceHub", "TryGetLocalHub" },
            new[] { "ReferenceHub", "roleManager" },
            new[] { "ReferenceHub", "playerStats" },
            new[] { "ReferenceHub", "nicknameSync" },
            new[] { "PlayerRoles.PlayerRoleManager", "CurrentRole" },
            new[] { "PlayerRoles.PlayerRoleBase", "Team" },
            new[] { "PlayerRoles.PlayerRoleBase", "RoleColor" },
            new[] { "PlayerRoles.PlayerRoleBase", "RoleName" },
            new[] { "PlayerRoles.FirstPersonControl.IFpcRole", "FpcModule" },
            new[] { "PlayerRoles.FirstPersonControl.FirstPersonMovementModule", "Position" },
            new[] { "PlayerRoles.FirstPersonControl.FirstPersonMovementModule", "ModuleReady" },
            new[] { "PlayerRoles.FirstPersonControl.FirstPersonMovementModule", "CharController" },
            new[] { "PlayerRoles.FirstPersonControl.FirstPersonMovementModule", "CharControllerSet" },
            new[] { "PlayerRoles.FirstPersonControl.FirstPersonMovementModule", "Motor" },
            new[] { "PlayerRoles.FirstPersonControl.FirstPersonMovementModule", "CharacterModelInstance" },
            new[] { "PlayerRoles.FirstPersonControl.FpcMotor", "IsInvisible" },
            new[] { "PlayerRoles.FirstPersonControl.Thirdperson.CharacterModel", "Hitboxes" },
            new[] { "HitboxIdentity", "HitboxType" },
            new[] { "HitboxIdentity", "CenterOfMass" },
            new[] { "HitboxIdentity", "IsEnemy" },
            new[] { "PlayerStatsSystem.PlayerStats", "GetModule" },
            new[] { "PlayerStatsSystem.StatBase", "CurValue" },
            new[] { "PlayerStatsSystem.StatBase", "MaxValue" },
            new[] { "PlayerStatsSystem.HealthStat", "CurValue" },
            new[] { "PlayerStatsSystem.HumeShieldStat", "CurValue" },
            new[] { "NicknameSync", "DisplayName" },
        };

        private static readonly string[][] ItemMembers =
        {
            new[] { "InventorySystem.Items.Pickups.ItemPickupBase", "Info" },
            new[] { "InventorySystem.Items.Pickups.ItemPickupBase", "Position" },
            new[] { "InventorySystem.Items.Pickups.ItemPickupBase", "OnPickupAdded" },
            new[] { "InventorySystem.Items.Pickups.ItemPickupBase", "OnPickupDestroyed" },
            new[] { "InventorySystem.Items.Pickups.PickupSyncInfo", "ItemId" },
            new[] { "InventorySystem.Items.Pickups.PickupSyncInfo", "Locked" },
            new[] { "InventorySystem.InventoryItemLoader", "TryGetItem" },
            new[] { "InventorySystem.Items.ItemBase", "Category" },
        };

        private static readonly string[][] CursorMembers =
        {
            new[] { "CursorManagement.CursorManager", "Register" },
            new[] { "CursorManagement.CursorManager", "Unregister" },
        };

        public static Report Validate()
        {
            var r = new Report();
            r.IsMono = Type.GetType("Mono.Runtime") != null;
            if (!r.IsMono)
                r.Problems.Add("Not running on the Mono runtime (Mono.Runtime type not found).");

            Assembly game;
            try
            {
                game = typeof(ReferenceHub).Assembly;
            }
            catch (Exception e)
            {
                r.Problems.Add("Assembly-CSharp could not be resolved: " + e.GetType().Name + ": " + e.Message);
                return r;
            }

            r.AssemblyName = game.GetName().Name;
            try { r.AssemblyLocation = game.Location; } catch { r.AssemblyLocation = "?"; }
            if (r.AssemblyName != "Assembly-CSharp")
                r.Problems.Add("ReferenceHub was resolved from '" + r.AssemblyName + "', expected 'Assembly-CSharp'.");

            try
            {
                byte major = GameCore.Version.Major, minor = GameCore.Version.Minor, rev = GameCore.Version.Revision;
                r.GameVersion = major + "." + minor + "." + rev + " (" + GameCore.Version.BuildType + ")";
                r.VersionMatches = major == RefMajor && minor == RefMinor && rev == RefRevision;
                if (!r.VersionMatches)
                    r.Problems.Add("Game version " + r.GameVersion + " differs from reference " +
                                   RefMajor + "." + RefMinor + "." + RefRevision + "; features run only where their members still exist.");
            }
            catch (Exception e)
            {
                r.Problems.Add("GameCore.Version unreadable: " + e.GetType().Name + ": " + e.Message);
            }

            r.PlayerEspAvailable = CheckMembers(game, PlayerMembers, "Player ESP", r.Problems);
            r.ItemEspAvailable = CheckMembers(game, ItemMembers, "Item ESP", r.Problems);
            r.CursorOverrideAvailable = CheckMembers(game, CursorMembers, "Cursor unlock", r.Problems);
            return r;
        }

        private static bool CheckMembers(Assembly game, string[][] members, string feature, List<string> problems)
        {
            bool ok = true;
            foreach (string[] m in members)
            {
                Type t = game.GetType(m[0], false);
                if (t == null)
                {
                    problems.Add(feature + ": type " + m[0] + " not found.");
                    ok = false;
                    continue;
                }

                if (!HasMember(t, m[1]))
                {
                    problems.Add(feature + ": member " + m[0] + "." + m[1] + " not found.");
                    ok = false;
                }
            }
            return ok;
        }

        private static bool HasMember(Type t, string name)
        {
            for (Type cur = t; cur != null; cur = cur.BaseType)
            {
                if (cur.GetMember(name, AnyMember).Length > 0)
                    return true;
            }
            return false;
        }
    }
}
