using System.Collections.Generic;
using InventorySystem.Items;
using InventorySystem.Items.Pickups;
using UnityEngine;

namespace SLInternal
{
    // Pickups are tracked through ItemPickupBase.OnPickupAdded/OnPickupDestroyed and,
    // because pickups that existed before injection never raise "added", resynced with
    // FindObjectsOfType every few seconds.
    internal sealed class ItemEsp
    {
        private const float ResyncSeconds = 3f;
        private const int MaxLabels = 200;

        private readonly Settings _s;
        private readonly WorldCache _world;
        private readonly HashSet<ItemPickupBase> _pickups = new HashSet<ItemPickupBase>();
        private readonly List<ItemPickupBase> _dead = new List<ItemPickupBase>();
        private readonly Dictionary<ItemType, string> _names = new Dictionary<ItemType, string>();
        private readonly Dictionary<ItemType, ItemCategory> _categories = new Dictionary<ItemType, ItemCategory>();
        private float _nextResync;
        private bool _hooked;

        public int Count => _pickups.Count;

        public ItemEsp(Settings s, WorldCache world)
        {
            _s = s;
            _world = world;
        }

        public void Attach()
        {
            GameApi.PickupAdded += OnAdded;
            GameApi.PickupRemoved += OnRemoved;
            GameApi.HookPickupEvents();
            _hooked = true;
            Resync();
        }

        public void Detach()
        {
            if (!_hooked)
                return;
            GameApi.UnhookPickupEvents();
            GameApi.PickupAdded -= OnAdded;
            GameApi.PickupRemoved -= OnRemoved;
            _hooked = false;
            _pickups.Clear();
        }

        private void OnAdded(ItemPickupBase p)
        {
            if (p != null)
                _pickups.Add(p);
        }

        private void OnRemoved(ItemPickupBase p) => _pickups.Remove(p);

        public void Update()
        {
            if (!_s.ItemEspEnabled)
                return;
            if (Time.unscaledTime >= _nextResync)
                Resync();
        }

        private void Resync()
        {
            _nextResync = Time.unscaledTime + ResyncSeconds;
            _dead.Clear();
            foreach (ItemPickupBase p in _pickups)
                if (p == null)
                    _dead.Add(p);
            foreach (ItemPickupBase p in _dead)
                _pickups.Remove(p);
            foreach (ItemPickupBase p in Object.FindObjectsOfType<ItemPickupBase>())
                _pickups.Add(p);
        }

        private bool Wanted(ItemCategory c)
        {
            switch (c)
            {
                case ItemCategory.Keycard: return _s.ItemKeycards;
                case ItemCategory.Firearm: return _s.ItemFirearms;
                case ItemCategory.Medical: return _s.ItemMedical;
                case ItemCategory.SCPItem: return _s.ItemScp;
                case ItemCategory.Grenade: return _s.ItemGrenades;
                case ItemCategory.Armor: return _s.ItemArmor;
                case ItemCategory.Ammo: return _s.ItemAmmo;
                default: return _s.ItemOther;
            }
        }

        private static Color CategoryColor(ItemCategory c)
        {
            switch (c)
            {
                case ItemCategory.Keycard: return new Color(1f, 0.85f, 0.2f);
                case ItemCategory.Firearm: return new Color(1f, 0.45f, 0.3f);
                case ItemCategory.Medical: return new Color(0.4f, 1f, 0.5f);
                case ItemCategory.SCPItem: return new Color(1f, 0.3f, 0.9f);
                case ItemCategory.Grenade: return new Color(1f, 0.65f, 0.2f);
                case ItemCategory.Armor: return new Color(0.6f, 0.75f, 1f);
                case ItemCategory.Ammo: return new Color(0.75f, 0.75f, 0.75f);
                default: return new Color(0.9f, 0.9f, 0.9f);
            }
        }

        private ItemCategory Category(ItemType t)
        {
            if (_categories.TryGetValue(t, out ItemCategory c))
                return c;
            if (GameApi.TryGetCategory(t, out c))
                _categories[t] = c; // only cache real answers; the item table may load later
            return c;
        }

        private string Name(ItemType t)
        {
            if (!_names.TryGetValue(t, out string n))
                _names[t] = n = t.ToString();
            return n;
        }

        public void Draw(Camera cam)
        {
            if (!_s.ItemEspEnabled || cam == null || _world.Local == null)
                return;
            Vector3 origin = cam.transform.position;
            float maxSq = _s.ItemMaxDistance * _s.ItemMaxDistance;
            int drawn = 0;
            foreach (ItemPickupBase p in _pickups)
            {
                if (p == null)
                    continue;
                Vector3 pos = p.transform.position;
                float dSq = (pos - origin).sqrMagnitude;
                if (dSq > maxSq || !WorldCache.Plausible(pos))
                    continue;
                ItemType type = GameApi.PickupType(p);
                ItemCategory cat = Category(type);
                if (!Wanted(cat))
                    continue;
                if (!ViewCamera.WorldToGui(cam, pos, out Vector2 gui))
                    continue;
                SLInternal.Draw.Text(gui, $"{Name(type)} {Mathf.Sqrt(dSq):0}m", CategoryColor(cat));
                if (++drawn >= MaxLabels)
                    break;
            }
        }
    }
}
