using System;
using System.Collections.Generic;
using System.Reflection;
using InventorySystem;
using InventorySystem.Items;
using InventorySystem.Items.Pickups;
using PlayerRoles;
using PlayerRoles.FirstPersonControl;
using PlayerStatsSystem;
using UnityEngine;

namespace SLInternal
{
    // The only file that touches game (Assembly-CSharp) members.
    //
    // Direct members: used by Northwood's LabAPI, which compiles against the normal
    // (non-publicized) Assembly-CSharp, so they are public on builds of that era
    // (LabAPI commit 7b837be, 2026-05-30). If your client build renamed one, the compiler
    // stops here and names it.
    //
    // Reflected members: only seen in EXILED, which compiles against a publicized
    // assembly, so their visibility is unknown. They are looked up by name at runtime,
    // reported in the menu's Status tab, and every one has a fallback.
    internal static class GameApi
    {
        private const BindingFlags AnyStatic = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        private const BindingFlags AnyInstance = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;

        // ---- direct ----

        public static HashSet<ReferenceHub> AllHubs => ReferenceHub.AllHubs;

        public static bool IsLocal(ReferenceHub hub) => hub.isLocalPlayer;

        public static PlayerRoleBase CurrentRole(ReferenceHub hub) => hub.roleManager != null ? hub.roleManager.CurrentRole : null;

        public static bool TryGetFpcPosition(PlayerRoleBase role, out Vector3 position)
        {
            if (role is IFpcRole fpc && fpc.FpcModule != null)
            {
                position = fpc.FpcModule.Position;
                return true;
            }
            position = default;
            return false;
        }

        public static Transform CameraTransform(ReferenceHub hub) => hub.PlayerCameraReference;

        public static string DisplayName(ReferenceHub hub) => hub.nicknameSync != null ? hub.nicknameSync.DisplayName : null;

        public static bool TryGetHealth(ReferenceHub hub, out float current, out float max)
        {
            current = max = 0f;
            if (hub.playerStats == null)
                return false;
            HealthStat hp = hub.playerStats.GetModule<HealthStat>();
            if (hp == null)
                return false;
            current = hp.CurValue;
            max = hp.MaxValue;
            return AimMath.IsFinite(current) && AimMath.IsFinite(max) && max > 0f;
        }

        public static Faction GetFaction(Team team) => team.GetFaction();

        public static ItemType PickupType(ItemPickupBase pickup) => pickup.Info.ItemId;

        public static bool TryGetCategory(ItemType type, out ItemCategory category)
        {
            if (InventoryItemLoader.AvailableItems != null &&
                InventoryItemLoader.AvailableItems.TryGetValue(type, out ItemBase item) && item != null)
            {
                category = item.Category;
                return true;
            }
            category = ItemCategory.None;
            return false;
        }

        // Method groups bind to whatever delegate type the game declares for these events.
        public static event Action<ItemPickupBase> PickupAdded;
        public static event Action<ItemPickupBase> PickupRemoved;

        private static void OnPickupAdded(ItemPickupBase p) => PickupAdded?.Invoke(p);

        private static void OnPickupDestroyed(ItemPickupBase p) => PickupRemoved?.Invoke(p);

        public static void HookPickupEvents()
        {
            ItemPickupBase.OnPickupAdded += OnPickupAdded;
            ItemPickupBase.OnPickupDestroyed += OnPickupDestroyed;
        }

        public static void UnhookPickupEvents()
        {
            ItemPickupBase.OnPickupAdded -= OnPickupAdded;
            ItemPickupBase.OnPickupDestroyed -= OnPickupDestroyed;
        }

        // ---- reflected ----

        private static Type _hitboxType;          // HitboxIdentity (global namespace)
        private static PropertyInfo _targetHubProp; // HitboxIdentity.TargetHub
        private static FieldInfo _targetHubField;
        private static MethodInfo _isEnemy;       // static bool HitboxIdentity.IsEnemy(RoleTypeId, RoleTypeId)
        private static MemberInfo _hitregMask;    // InventorySystem.Items.Firearms.Modules.HitscanHitregModuleBase.HitregMask
        private static int _mask;
        private static bool _maskFromGame;
        private static readonly object[] IsEnemyArgs = new object[2];

        public static string[] Status { get; private set; } = Array.Empty<string>();

        public static int RaycastMask => _mask;

        public static void Resolve()
        {
            Assembly asm = typeof(ReferenceHub).Assembly;

            _hitboxType = asm.GetType("HitboxIdentity", false);
            if (_hitboxType != null)
            {
                _targetHubProp = _hitboxType.GetProperty("TargetHub", AnyInstance);
                if (_targetHubProp == null || _targetHubProp.PropertyType != typeof(ReferenceHub) || _targetHubProp.GetGetMethod(true) == null)
                {
                    _targetHubProp = null;
                    _targetHubField = _hitboxType.GetField("TargetHub", AnyInstance);
                    if (_targetHubField != null && _targetHubField.FieldType != typeof(ReferenceHub))
                        _targetHubField = null;
                }
                foreach (MethodInfo m in _hitboxType.GetMethods(AnyStatic))
                {
                    if (m.Name != "IsEnemy" || m.ReturnType != typeof(bool))
                        continue;
                    ParameterInfo[] p = m.GetParameters();
                    if (p.Length == 2 && p[0].ParameterType == typeof(RoleTypeId) && p[1].ParameterType == typeof(RoleTypeId))
                    {
                        _isEnemy = m;
                        break;
                    }
                }
            }

            Type hitreg = asm.GetType("InventorySystem.Items.Firearms.Modules.HitscanHitregModuleBase", false);
            if (hitreg != null)
                _hitregMask = (MemberInfo)hitreg.GetField("HitregMask", AnyStatic) ?? hitreg.GetProperty("HitregMask", AnyStatic);
            RefreshMask();

            Status = new[]
            {
                Line("HitboxIdentity type", _hitboxType != null, "player hits are detected by ReferenceHub in parents"),
                Line("HitboxIdentity.TargetHub", _targetHubProp != null || _targetHubField != null, "GetComponentInParent<ReferenceHub>"),
                Line("HitboxIdentity.IsEnemy(RoleTypeId,RoleTypeId)", _isEnemy != null, "faction comparison"),
                Line("HitscanHitregModuleBase.HitregMask", _maskFromGame, "Physics.DefaultRaycastLayers"),
            };
            foreach (string s in Status)
                Log.Info(s);
        }

        private static string Line(string what, bool ok, string fallback) =>
            ok ? what + ": resolved" : what + ": NOT FOUND, using " + fallback;

        private static void RefreshMask()
        {
            _maskFromGame = false;
            _mask = Physics.DefaultRaycastLayers;
            if (_hitregMask == null)
                return;
            try
            {
                object v = _hitregMask is FieldInfo f ? f.GetValue(null) : ((PropertyInfo)_hitregMask).GetValue(null, null);
                if (TryToMask(v, out int mask) && mask != 0)
                {
                    _mask = mask;
                    _maskFromGame = true;
                }
            }
            catch (Exception e)
            {
                Log.Error("reading HitregMask: " + e.Message);
            }
        }

        private static bool TryToMask(object v, out int mask)
        {
            mask = 0;
            switch (v)
            {
                case null: return false;
                case int i: mask = i; return true;
                case LayerMask lm: mask = lm.value; return true;
            }
            // Wrapper type: look for an implicit conversion to int or LayerMask.
            foreach (MethodInfo m in v.GetType().GetMethods(BindingFlags.Static | BindingFlags.Public))
            {
                if (m.Name != "op_Implicit" || m.GetParameters().Length != 1 || m.GetParameters()[0].ParameterType != v.GetType())
                    continue;
                object r = m.Invoke(null, new[] { v });
                if (r is int ri) { mask = ri; return true; }
                if (r is LayerMask rl) { mask = rl.value; return true; }
            }
            return false;
        }

        // Resolves the player a collider belongs to. isHitbox is true for real hit
        // colliders (HitboxIdentity); false for e.g. the CharacterController capsule.
        public static ReferenceHub HubFromCollider(Collider collider, out bool isHitbox)
        {
            if (_hitboxType == null)
            {
                ReferenceHub any = collider.GetComponentInParent<ReferenceHub>();
                isHitbox = any != null;
                return any;
            }

            Component hitbox = collider.GetComponent(_hitboxType);
            isHitbox = hitbox != null;
            if (!isHitbox)
                return collider.GetComponentInParent<ReferenceHub>();

            ReferenceHub hub = null;
            if (_targetHubProp != null) hub = _targetHubProp.GetValue(hitbox, null) as ReferenceHub;
            else if (_targetHubField != null) hub = _targetHubField.GetValue(hitbox) as ReferenceHub;
            return hub != null ? hub : hitbox.GetComponentInParent<ReferenceHub>();
        }

        public static bool IsEnemy(RoleTypeId attacker, Team attackerTeam, RoleTypeId target, Team targetTeam)
        {
            if (_isEnemy != null)
            {
                try
                {
                    IsEnemyArgs[0] = attacker;
                    IsEnemyArgs[1] = target;
                    return (bool)_isEnemy.Invoke(null, IsEnemyArgs);
                }
                catch (Exception e)
                {
                    Log.Error("HitboxIdentity.IsEnemy failed, switching to faction comparison: " + e.Message);
                    _isEnemy = null;
                }
            }
            return GetFaction(attackerTeam) != GetFaction(targetTeam);
        }
    }
}
