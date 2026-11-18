using UnityEngine;

namespace SLMenu
{
    /// <summary>Runtime options edited from the menu. Kept in memory only.</summary>
    internal sealed class Settings
    {
        public KeyCode MenuKey = KeyCode.Insert;

        // Player ESP
        public bool PlayerEsp = true;
        public bool PlayerBox = true;
        public bool PlayerName = true;
        public bool PlayerRole = true;
        public bool PlayerDistance = true;
        public bool PlayerHealth = true;
        public bool PlayerHeadDot = true;
        public bool PlayerSnaplines;
        public bool ShowTeammates;
        public bool ShowCulled;
        public bool UseRoleColor = true;
        public float PlayerMaxDistance = 150f;

        // Item ESP
        public bool ItemEsp = true;
        public bool ItemDistance = true;
        public bool ItemShowLocked = true;
        public float ItemMaxDistance = 40f;

        /// <summary>Indexed by (byte)ItemCategory; ItemCategory has 10 values in the reference build.</summary>
        public readonly bool[] ItemCategoryEnabled = { true, true, true, true, true, true, true, true, true, true };

        public bool IsCategoryEnabled(byte category)
        {
            return category < ItemCategoryEnabled.Length ? ItemCategoryEnabled[category] : true;
        }
    }
}
