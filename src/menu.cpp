#include "menu.h"

#include "esp.h"
#include "game.h"
#include "gui.h"
#include "settings.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

namespace slm {
Settings settings;
}

namespace slm::menu {

using game::refs;

namespace {

enum class Kind { Header, Toggle, Slider, Action, Info };

struct Item {
    Kind kind;
    std::string label;
    bool* flag = nullptr;
    float* value = nullptr;
    float min = 0, max = 0, step = 0;
    bool available = true;
};

std::atomic<bool> g_unload{false};
int g_selected = 1;
bool g_keyState[256] = {};

bool Pressed(int vk)
{
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool edge = down && !g_keyState[vk & 0xFF];
    g_keyState[vk & 0xFF] = down;
    return edge;
}

bool GameHasFocus()
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg)
        GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

std::vector<Item> BuildItems()
{
    Settings& s = settings;
    std::vector<Item> items;
    auto header = [&](const char* t) { items.push_back({Kind::Header, t}); };
    auto toggle = [&](const char* t, bool* f, bool avail = true) { items.push_back({Kind::Toggle, t, f, nullptr, 0, 0, 0, avail}); };
    auto slider = [&](const char* t, float* v, float mn, float mx, float st) { items.push_back({Kind::Slider, t, nullptr, v, mn, mx, st}); };
    auto info = [&](const std::string& t) { items.push_back({Kind::Info, t}); };

    header("PLAYERS");
    toggle("Player ESP", &s.playerEsp, refs.players);
    toggle("Box", &s.playerBox, refs.box);
    toggle("Head dot", &s.playerHead, refs.head);
    toggle("Name", &s.playerName, refs.players);
    toggle("Role", &s.playerRole, refs.players);
    toggle("Distance", &s.playerDistance, refs.players);
    toggle("Health (only while you are dead)", &s.playerHealth, refs.health);
    toggle("Snaplines", &s.playerSnaplines, gui::CanDrawLines());
    toggle("Show teammates", &s.showTeammates, refs.teamFilter);
    toggle("Show last known (server-culled)", &s.showCulled, refs.players);
    toggle("Game role colours", &s.roleColors, refs.players);
    slider("Max distance", &s.playerMaxDistance, 10, 500, 10);

    header("ITEMS");
    toggle("Item ESP", &s.itemEsp, refs.items);
    toggle("Distance", &s.itemDistance, refs.items);
    toggle("Show locked pickups", &s.itemShowLocked, refs.items && refs.pickupFlagLocked >= 0);
    slider("Max distance", &s.itemMaxDistance, 5, 300, 5);
    for (size_t i = 0; i < refs.itemCategories.size() && i < 32; ++i)
        toggle(("  " + refs.itemCategories[i].name).c_str(), &s.categoryEnabled[i], refs.items && refs.itemCategory);

    header("INFO");
    char line[256];
    std::snprintf(line, sizeof(line), "Players drawn %d, withheld by server %d, errors %d%s", esp::playerStats.drawn,
                  esp::playerStats.culled, esp::playerStats.errors, esp::playerStats.disabled ? " (STOPPED)" : "");
    info(line);
    if (!esp::playerStats.lastError.empty())
        info("  last: " + esp::playerStats.lastError.substr(0, 90));
    std::snprintf(line, sizeof(line), "Items drawn %d, errors %d%s", esp::itemStats.drawn, esp::itemStats.errors,
                  esp::itemStats.disabled ? " (STOPPED)" : "");
    info(line);
    if (!esp::itemStats.lastError.empty())
        info("  last: " + esp::itemStats.lastError.substr(0, 90));
    std::snprintf(line, sizeof(line), "Projection error vs Unity: %s px", esp::diagnostics.projectionErrorPx < 0 ? "n/a" :
                  std::to_string(esp::diagnostics.projectionErrorPx).substr(0, 6).c_str());
    info(line);
    std::snprintf(line, sizeof(line), "Camera.main vs MainCameraController: %s m", esp::diagnostics.cameraOffset < 0 ? "n/a" :
                  std::to_string(esp::diagnostics.cameraOffset).substr(0, 6).c_str());
    info(line);
    for (size_t i = 0; i < refs.problems.size() && i < 8; ++i)
        info("! " + refs.problems[i].substr(0, 90));
    if (refs.problems.size() > 8)
        info("! ... see SLMenu.log");
    items.push_back({Kind::Action, "Unload (End)"});
    return items;
}

bool Selectable(const Item& it)
{
    return (it.kind == Kind::Toggle || it.kind == Kind::Slider || it.kind == Kind::Action) && it.available;
}

void Activate(Item& it, int direction)
{
    switch (it.kind) {
    case Kind::Toggle:
        *it.flag = !*it.flag;
        break;
    case Kind::Slider:
        *it.value = std::clamp(*it.value + it.step * static_cast<float>(direction == 0 ? 1 : direction), it.min, it.max);
        break;
    case Kind::Action:
        g_unload = true;
        break;
    default:
        break;
    }
}

} // namespace

bool UnloadRequested()
{
    return g_unload.load();
}

void HandleInput()
{
    Settings& s = settings;
    const bool focus = GameHasFocus();
    const bool menuKey = Pressed(s.keyMenu);
    const bool unloadKey = Pressed(s.keyUnload);
    const bool up = Pressed(s.keyUp), down = Pressed(s.keyDown);
    const bool left = Pressed(s.keyLeft), right = Pressed(s.keyRight), enter = Pressed(s.keyToggle);
    if (!focus)
        return;
    if (menuKey)
        s.menuOpen = !s.menuOpen;
    if (unloadKey)
        g_unload = true;
    if (!s.menuOpen)
        return;

    std::vector<Item> items = BuildItems();
    const int n = static_cast<int>(items.size());
    auto step = [&](int dir) {
        for (int i = 0; i < n; ++i) {
            g_selected = (g_selected + dir + n) % n;
            if (Selectable(items[g_selected]))
                return;
        }
    };
    if (g_selected >= n || !Selectable(items[g_selected]))
        step(1);
    if (up) step(-1);
    if (down) step(1);
    if (g_selected < n && Selectable(items[g_selected])) {
        if (enter) Activate(items[g_selected], 0);
        if (left) Activate(items[g_selected], items[g_selected].kind == Kind::Toggle ? 0 : -1);
        if (right) Activate(items[g_selected], items[g_selected].kind == Kind::Toggle ? 0 : 1);
    }
}

void Draw()
{
    const Settings& s = settings;
    if (!s.menuOpen)
        return;
    const std::vector<Item> items = BuildItems();

    const float x = 20.0f, y0 = 60.0f, width = 380.0f, lh = 16.0f;
    const float height = 26.0f + lh * static_cast<float>(items.size()) + 8.0f;
    gui::FillRect({x, y0, width, height}, {0.05f, 0.05f, 0.07f, 0.82f});
    gui::FillRect({x, y0, width, 22.0f}, {0.15f, 0.35f, 0.75f, 0.95f});
    gui::Text(x + 8.0f, y0 + 4.0f, "SLMenu   Insert: hide   Up/Down  Left/Right  Enter", {1, 1, 1, 1}, false);

    float y = y0 + 26.0f;
    for (int i = 0; i < static_cast<int>(items.size()); ++i, y += lh) {
        const Item& it = items[i];
        const bool sel = i == g_selected;
        if (sel)
            gui::FillRect({x + 2.0f, y - 1.0f, width - 4.0f, lh}, {0.3f, 0.45f, 0.9f, 0.45f});
        gui::Color c = it.available ? gui::Color{0.92f, 0.92f, 0.92f, 1} : gui::Color{0.5f, 0.5f, 0.5f, 1};
        std::string text;
        switch (it.kind) {
        case Kind::Header:
            c = {0.55f, 0.75f, 1, 1};
            text = it.label;
            break;
        case Kind::Toggle:
            text = std::string(*it.flag ? "[x] " : "[ ] ") + it.label + (it.available ? "" : "  (n/a)");
            break;
        case Kind::Slider:
            text = it.label + ": < " + std::to_string(static_cast<int>(*it.value)) + " m >";
            break;
        case Kind::Action:
            c = {1, 0.6f, 0.5f, 1};
            text = it.label;
            break;
        case Kind::Info:
            c = it.label.rfind("! ", 0) == 0 ? gui::Color{1, 0.75f, 0.4f, 1} : gui::Color{0.75f, 0.75f, 0.75f, 1};
            text = it.label;
            break;
        }
        gui::Text(x + 10.0f, y, text, c, false);
    }
}

} // namespace slm::menu
