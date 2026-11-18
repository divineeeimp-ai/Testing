using System.Collections.Generic;
using InventorySystem;
using InventorySystem.Items;
using InventorySystem.Items.Pickups;
using SLMenu.Render;
using UnityEngine;

namespace SLMenu.Esp
{
    /// <summary>Item pickup ESP: ItemType name, distance, colour by ItemCategory.</summary>
    internal sealed class ItemEsp
    {
        private const int MaxDrawn = 512;
        private const float MaxCoordinate = 20000f;

        private readonly PickupTracker _tracker;
        private readonly Dictionary<ItemType, ItemCategory> _categoryCache = new Dictionary<ItemType, ItemCategory>();

        public int Drawn { get; private set; }

        public ItemEsp(PickupTracker tracker)
        {
            _tracker = tracker;
        }

        public void Render(Camera cam, Settings s)
        {
            Drawn = 0;
            _tracker.Prune();

            Vector3 eye = cam.transform.position;
            float screenW = Screen.width, screenH = Screen.height;

            foreach (ItemPickupBase pickup in _tracker.Pickups)
            {
                if (Drawn >= MaxDrawn)
                    break;
                if (pickup == null)
                    continue;

                PickupSyncInfo info = pickup.Info;
                ItemType type = info.ItemId;
                if (type == ItemType.None)
                    continue;
                if (info.Locked && !s.ItemShowLocked)
                    continue;

                ItemCategory category = GetCategory(type);
                if (!s.IsCategoryEnabled((byte)category))
                    continue;

                Vector3 pos = pickup.Position;
                if (!IsSane(pos))
                    continue;
                float distance = Vector3.Distance(eye, pos);
                if (!(distance <= s.ItemMaxDistance))
                    continue;

                if (!ViewCamera.WorldToGui(cam, pos, out Vector2 gui))
                    continue;
                if (!ScreenMath.IntersectsScreen(gui.x, gui.y, 0f, 0f, screenW, screenH))
                    continue;

                Color c = CategoryColor(category);
                string label = s.ItemDistance ? type + " " + Mathf.RoundToInt(distance) + "m" : type.ToString();
                Draw.Dot(gui, 3f, c);
                Draw.Text(gui.x, gui.y + 3f, label, c);
                Drawn++;
            }
        }

        private ItemCategory GetCategory(ItemType type)
        {
            if (_categoryCache.TryGetValue(type, out ItemCategory cached))
                return cached;

            ItemCategory category = ItemCategory.None;
            if (InventoryItemLoader.TryGetItem(type, out ItemBase template) && template != null)
                category = template.Category;
            _categoryCache[type] = category;
            return category;
        }

        public static Color CategoryColor(ItemCategory c)
        {
            switch (c)
            {
                case ItemCategory.Keycard: return new Color(1f, 0.85f, 0.2f);
                case ItemCategory.Medical: return new Color(0.3f, 1f, 0.4f);
                case ItemCategory.Radio: return new Color(0.7f, 0.7f, 0.7f);
                case ItemCategory.Firearm: return new Color(1f, 0.35f, 0.25f);
                case ItemCategory.Grenade: return new Color(1f, 0.55f, 0.1f);
                case ItemCategory.SCPItem: return new Color(0.85f, 0.35f, 1f);
                case ItemCategory.SpecialWeapon: return new Color(1f, 0.2f, 0.6f);
                case ItemCategory.Ammo: return new Color(0.6f, 0.75f, 1f);
                case ItemCategory.Armor: return new Color(0.35f, 0.65f, 1f);
                default: return Color.white;
            }
        }

        private static bool IsSane(Vector3 v)
        {
            return ScreenMath.IsFinite(v.x) && ScreenMath.IsFinite(v.y) && ScreenMath.IsFinite(v.z) &&
                   Mathf.Abs(v.x) < MaxCoordinate && Mathf.Abs(v.y) < MaxCoordinate && Mathf.Abs(v.z) < MaxCoordinate;
        }
    }
}
