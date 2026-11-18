using CursorManagement;
using UnityEngine;

namespace SLMenu
{
    /// <summary>
    /// Frees the cursor and locks movement while the menu is open, through the game's own
    /// CursorManager: every LateUpdate it takes the highest CursorOverrideMode of all registered
    /// overrides; Free (3) outranks the first-person module's Centered (1) and maps to an unlocked,
    /// visible cursor. Kept in its own component so a changed cursor API cannot break the rest of the menu.
    /// </summary>
    internal sealed class CursorUnlock : MonoBehaviour, ICursorOverride
    {
        public bool Active { get; set; }

        public CursorOverrideMode CursorOverride => Active ? CursorOverrideMode.Free : CursorOverrideMode.NoOverride;

        public bool LockMovement => Active;

        private void OnEnable()
        {
            CursorManager.Register(this);
        }

        private void OnDisable()
        {
            Active = false;
            CursorManager.Unregister(this);
        }
    }
}
