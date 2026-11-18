#pragma once
// Runtime options, edited from the in-game menu and kept in memory only.

#include <windows.h>

namespace slm {

struct Settings {
    // keys (virtual-key codes)
    int keyMenu = VK_INSERT;
    int keyUp = VK_UP;
    int keyDown = VK_DOWN;
    int keyLeft = VK_LEFT;
    int keyRight = VK_RIGHT;
    int keyToggle = VK_RETURN;
    int keyUnload = VK_END;

    bool menuOpen = true;

    // player ESP
    bool playerEsp = true;
    bool playerBox = true;
    bool playerHead = true;
    bool playerName = true;
    bool playerRole = true;
    bool playerDistance = true;
    bool playerHealth = true;
    bool playerSnaplines = false;
    bool showTeammates = false;
    bool showCulled = false;
    bool roleColors = true;
    float playerMaxDistance = 150.0f;

    // item ESP
    bool itemEsp = true;
    bool itemDistance = true;
    bool itemShowLocked = true;
    float itemMaxDistance = 40.0f;
    bool categoryEnabled[32] = {true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true,
                                true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true};
};

extern Settings settings;

} // namespace slm
