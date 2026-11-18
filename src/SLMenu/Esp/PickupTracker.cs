using System.Collections.Generic;
using InventorySystem.Items.Pickups;
using UnityEngine;

namespace SLMenu.Esp
{
    /// <summary>
    /// Live set of item pickups. The game keeps no static list of pickups; it raises
    /// ItemPickupBase.OnPickupAdded from Start() and OnPickupDestroyed from OnDestroy() on every
    /// instance (client included). Pickups that already existed at injection are seeded once.
    /// The game's CustomInterestManagement makes every networked object visible to every ready
    /// client, so all pickups on the map exist on the client regardless of distance.
    /// </summary>
    internal sealed class PickupTracker
    {
        public const int MaxTracked = 4096;

        private readonly HashSet<ItemPickupBase> _pickups = new HashSet<ItemPickupBase>();
        private bool _subscribed;

        public IEnumerable<ItemPickupBase> Pickups => _pickups;
        public int Count => _pickups.Count;

        public void Start()
        {
            if (_subscribed)
                return;
            ItemPickupBase.OnPickupAdded += Add;
            ItemPickupBase.OnPickupDestroyed += Remove;
            _subscribed = true;

            foreach (ItemPickupBase p in Object.FindObjectsOfType<ItemPickupBase>())
                Add(p);
        }

        public void Stop()
        {
            if (_subscribed)
            {
                ItemPickupBase.OnPickupAdded -= Add;
                ItemPickupBase.OnPickupDestroyed -= Remove;
                _subscribed = false;
            }
            _pickups.Clear();
        }

        /// <summary>Drops instances Unity has destroyed without our callback (e.g. scene unload).</summary>
        public void Prune()
        {
            _pickups.RemoveWhere(p => p == null);
        }

        private void Add(ItemPickupBase p)
        {
            if (p != null && _pickups.Count < MaxTracked)
                _pickups.Add(p);
        }

        private void Remove(ItemPickupBase p)
        {
            _pickups.Remove(p);
        }
    }
}
