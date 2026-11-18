#include "esp.h"

#include "game.h"
#include "gui.h"
#include "log.h"
#include "settings.h"

#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace slm::esp {

using il2cpp::api;
using game::refs;

Stats playerStats;
Stats itemStats;
Diagnostics diagnostics;

namespace {

constexpr uint32_t kMaxHubs = 256;
constexpr uint32_t kMaxPickups = 8192;
constexpr uint32_t kMaxHitboxes = 64;
constexpr uint32_t kMaxStatModules = 32;
constexpr int kDisableAfterFailingFrames = 300;

int g_frame = 0;

bool FindObjects(Il2CppClass* klass, uint32_t maxCount, std::vector<Il2CppObject*>& out)
{
    Il2CppObject* type = rt::TypeObject(klass);
    if (!type)
        return false;
    Il2CppObject* array;
    if (refs.findObjectsByType) {
        int32_t mode = static_cast<int32_t>(refs.sortModeNone);
        void* args[] = {type, &mode};
        array = rt::Invoke(refs.findObjectsByType, nullptr, args);
    } else {
        void* args[] = {type};
        array = rt::Invoke(refs.findObjectsOfType, nullptr, args);
    }
    return rt::ArrayElements(array, maxCount, out);
}

bool Alive(Il2CppObject* o)
{
    return rt::IsUnityAlive(o);
}

int64_t ReadBoxedEnum(Il2CppObject* boxed, const game::EnumKind& kind)
{
    if (!boxed)
        throw rt::Error("null enum");
    return kind.Read(api.object_unbox(boxed));
}

int64_t TeamOf(Il2CppObject* role)
{
    return ReadBoxedEnum(rt::Invoke(refs.roleTeam, role, nullptr, true), refs.teamKind);
}

gui::Color TeamColor(int64_t team)
{
    const char* name = rt::EnumName(refs.teams, team);
    if (!name)
        return {1, 1, 1, 1};
    if (!std::strcmp(name, "SCPs")) return {0.93f, 0.13f, 0.13f, 1};
    if (!std::strcmp(name, "FoundationForces")) return {0.2f, 0.55f, 1, 1};
    if (!std::strcmp(name, "ChaosInsurgency")) return {0.1f, 0.75f, 0.25f, 1};
    if (!std::strcmp(name, "Scientists")) return {1, 0.95f, 0.5f, 1};
    if (!std::strcmp(name, "ClassD")) return {1, 0.55f, 0.1f, 1};
    if (!std::strcmp(name, "Flamingos")) return {1, 0.45f, 0.75f, 1};
    return {1, 1, 1, 1};
}

void NoteFrame(Stats& s, int candidates)
{
    if (s.errors > 0 && s.drawn == 0 && candidates > 0)
        ++s.failingFrames;
    else
        s.failingFrames = 0;
    if (s.failingFrames > kDisableAfterFailingFrames && !s.disabled) {
        s.disabled = true;
        SLM_LOG("feature disabled after repeated errors: %s", s.lastError.c_str());
    }
}

bool TryBox(const View& v, Il2CppObject* module, Rect2& box)
{
    if (!rt::InvokeValue<bool>(refs.fpmCharControllerSet, module))
        return false;
    Il2CppObject* cc = rt::Invoke(refs.fpmCharController, module);
    if (!Alive(cc))
        return false;
    Il2CppObject* tr = rt::Invoke(refs.componentGetTransform, cc);
    if (!Alive(tr))
        return false;

    // FpcStateProcessor changes height and center while crouching: read them every frame.
    const Vec3 scale = rt::InvokeValue<Vec3>(refs.transformGetLossyScale, tr);
    const float half = rt::InvokeValue<float>(refs.ccHeight, cc) * 0.5f * std::fabs(scale.y);
    const float radius = rt::InvokeValue<float>(refs.ccRadius, cc) * std::fmax(std::fabs(scale.x), std::fabs(scale.z));
    if (!(half > 0.05f && half < 10.0f) || !(radius > 0.01f && radius < 10.0f))
        return false;
    Vec3 localCenter = rt::InvokeValue<Vec3>(refs.ccCenter, cc);
    void* args[] = {&localCenter};
    const Vec3 center = rt::InvokeValue<Vec3>(refs.transformTransformPoint, tr, args);
    if (!Sane(center))
        return false;

    Vec2 top, bottom;
    if (!Project(v, {center.x, center.y + half, center.z}, top) || !Project(v, {center.x, center.y - half, center.z}, bottom))
        return false;
    return BoxFromVertical(top, bottom, radius / half, box);
}

bool TryHead(const View& v, Il2CppObject* module, Vec2& head)
{
    Il2CppObject* model = rt::Invoke(refs.fpmCharacterModel, module);
    if (!Alive(model))
        return false;
    std::vector<Il2CppObject*> hitboxes;
    if (!rt::ArrayElements(rt::Invoke(refs.modelHitboxes, model), kMaxHitboxes, hitboxes))
        return false;
    for (Il2CppObject* hb : hitboxes) {
        if (!Alive(hb))
            continue;
        if (ReadBoxedEnum(rt::Invoke(refs.hitboxType, hb), refs.hitboxKind) != refs.hitboxHeadshot)
            continue;
        const Vec3 p = rt::InvokeValue<Vec3>(refs.hitboxCenter, hb);
        return Sane(p) && Project(v, p, head);
    }
    return false;
}

std::string HealthText(Il2CppObject* hub)
{
    Il2CppObject* stats = rt::GetField<Il2CppObject*>(hub, refs.hubPlayerStats);
    if (!Alive(stats))
        return {};
    std::vector<Il2CppObject*> modules;
    if (!rt::ArrayElements(rt::Invoke(refs.statsModules, stats), kMaxStatModules, modules))
        return {};
    std::string text;
    for (Il2CppObject* m : modules) {
        if (!m)
            continue;
        Il2CppClass* k = api.object_get_class(m);
        if (k == refs.healthStat) {
            const float cur = rt::InvokeValue<float>(refs.statCur, m, nullptr, true);
            const float max = rt::InvokeValue<float>(refs.statMax, m, nullptr, true);
            text = "HP " + std::to_string(static_cast<int>(std::ceil(cur))) + "/" +
                   std::to_string(static_cast<int>(std::ceil(max))) + text;
        } else if (k == refs.humeShieldStat && refs.humeShieldStat) {
            const float cur = rt::InvokeValue<float>(refs.statCur, m, nullptr, true);
            if (cur > 0.0f)
                text += "  HS " + std::to_string(static_cast<int>(std::ceil(cur)));
        }
    }
    return text;
}

void UpdateDiagnostics(const View& v)
{
    if (g_frame % 30 != 0)
        return;
    // A point 10 m ahead of the camera and slightly off-axis. Camera forward = -(row 2 of worldToCamera).
    const Vec3 forward = {-At(v.view, 2, 0), -At(v.view, 2, 1), -At(v.view, 2, 2)};
    Vec3 p = {v.eye.x + forward.x * 10.0f + 1.0f, v.eye.y + forward.y * 10.0f + 0.5f, v.eye.z + forward.z * 10.0f};
    Vec3 ours{};
    if (refs.cameraWorldToViewport && WorldToViewport(v.view, v.proj, p, ours)) {
        void* args[] = {&p};
        const Vec3 unity = rt::InvokeValue<Vec3>(refs.cameraWorldToViewport, v.camera, args);
        const float dx = (ours.x - unity.x) * v.w, dy = (ours.y - unity.y) * v.h;
        diagnostics.projectionErrorPx = std::sqrt(dx * dx + dy * dy);
    }
    if (refs.mccLastPosition)
        diagnostics.cameraOffset = Distance(v.eye, rt::InvokeValue<Vec3>(refs.mccLastPosition, nullptr));
}

} // namespace

bool BuildView(View& v)
{
    ++g_frame;
    v.camera = rt::Invoke(refs.cameraGetMain, nullptr);
    if (!Alive(v.camera))
        return false;
    v.view = rt::InvokeValue<Mat4>(refs.cameraWorldToCamera, v.camera);
    v.proj = rt::InvokeValue<Mat4>(refs.cameraProjection, v.camera);
    v.nearClip = rt::InvokeValue<float>(refs.cameraNearClip, v.camera);
    Il2CppObject* tr = rt::Invoke(refs.componentGetTransform, v.camera);
    if (!Alive(tr))
        return false;
    v.eye = rt::InvokeValue<Vec3>(refs.transformGetPosition, tr);
    v.w = gui::ScreenW();
    v.h = gui::ScreenH();
    if (!Sane(v.eye))
        return false;
    try {
        UpdateDiagnostics(v);
    } catch (const rt::Error&) {
        diagnostics = {};
    }
    return true;
}

bool Project(const View& v, Vec3 world, Vec2& gui)
{
    Vec3 vp;
    return WorldToViewport(v.view, v.proj, world, vp) && ViewportToGui(vp, v.nearClip, v.w, v.h, gui);
}

void DrawPlayers(const View& v)
{
    Stats& s = playerStats;
    s.drawn = s.culled = s.errors = 0;
    if (s.disabled || !refs.players || !settings.playerEsp)
        return;

    std::vector<Il2CppObject*> hubs;
    if (!FindObjects(refs.referenceHub, kMaxHubs, hubs))
        return;

    Il2CppObject* local = nullptr;
    if (refs.localHub) {
        void* args[] = {&local};
        rt::Invoke(refs.hubTryGetLocal, nullptr, args);
    }
    int64_t localTeam = refs.teamDead;
    if (Alive(local)) {
        Il2CppObject* rm = rt::GetField<Il2CppObject*>(local, refs.hubRoleManager);
        Il2CppObject* role = Alive(rm) ? rt::Invoke(refs.roleManagerCurrentRole, rm) : nullptr;
        if (Alive(role))
            localTeam = TeamOf(role);
    }
    const bool localAlive = localTeam != refs.teamDead;
    const Vec2 snapOrigin = {v.w * 0.5f, v.h};

    for (Il2CppObject* hub : hubs) {
        if (!Alive(hub) || hub == local)
            continue;
        try {
            Il2CppObject* rm = rt::GetField<Il2CppObject*>(hub, refs.hubRoleManager);
            if (!Alive(rm))
                continue;
            Il2CppObject* role = rt::Invoke(refs.roleManagerCurrentRole, rm);
            if (!Alive(role) || !api.class_is_assignable_from(refs.fpcRoleInterface, api.object_get_class(role)))
                continue; // spectators, SCP-079, unspawned: no body in the world
            Il2CppObject* module = rt::Invoke(refs.fpcRoleModule, role, nullptr, true);
            if (!Alive(module) || !rt::InvokeValue<bool>(refs.fpmModuleReady, module))
                continue;

            int64_t team = TeamOf(role);
            if (refs.teamFilter && localAlive && !settings.showTeammates) {
                int64_t a = localTeam, b = team;
                void* args[] = {&a, &b};
                if (!rt::InvokeValue<bool>(refs.hitboxIsEnemy, nullptr, args))
                    continue;
            }

            Il2CppObject* motor = rt::Invoke(refs.fpmMotor, module);
            const bool culled = motor && rt::InvokeValue<bool>(refs.motorIsInvisible, motor);
            if (culled) {
                ++s.culled;
                if (!settings.showCulled)
                    continue;
            }

            const Vec3 pos = rt::InvokeValue<Vec3>(refs.fpmPosition, module);
            if (!Sane(pos))
                continue;
            const float distance = Distance(v.eye, pos);
            if (!(distance <= settings.playerMaxDistance))
                continue;
            if (!refs.localHub && distance < 1.0f)
                continue; // without TryGetLocalHub, the body under the camera is assumed to be ours

            gui::Color color;
            if (settings.roleColors) {
                const auto c = rt::InvokeValue<rt::UColor>(refs.roleColor, role, nullptr, true);
                color = {c.r, c.g, c.b, 1.0f};
            } else {
                color = TeamColor(team);
            }
            if (culled)
                color.a = 0.4f;

            Rect2 box{};
            const bool hasBox = settings.playerBox && refs.box && TryBox(v, module, box);
            Vec2 anchor;
            if (hasBox) {
                if (!IntersectsScreen(box, v.w, v.h))
                    continue;
                anchor = {box.x + box.w * 0.5f, box.y + box.h};
            } else {
                if (!Project(v, pos, anchor) || !IntersectsScreen({anchor.x, anchor.y, 0, 0}, v.w, v.h))
                    continue;
            }

            if (settings.playerSnaplines)
                gui::Line(snapOrigin, anchor, color, 1.0f);
            if (hasBox)
                gui::BoxOutline(box, color);
            Vec2 head;
            if (settings.playerHead && refs.head && TryHead(v, module, head))
                gui::Dot(head, 4.0f, color);

            float y = anchor.y + 2.0f;
            if (settings.playerName) {
                Il2CppObject* nick = rt::GetField<Il2CppObject*>(hub, refs.hubNicknameSync);
                if (Alive(nick)) {
                    gui::Text(anchor.x, y, reinterpret_cast<Il2CppString*>(rt::Invoke(refs.nickDisplayName, nick)), color, true);
                    y += gui::kLineHeight;
                }
            }
            std::string info;
            if (settings.playerRole)
                info = rt::ToUtf8(reinterpret_cast<Il2CppString*>(rt::Invoke(refs.roleName, role)));
            if (settings.playerDistance)
                info += (info.empty() ? "" : "  ") + std::to_string(static_cast<int>(distance + 0.5f)) + "m";
            if (culled)
                info += (info.empty() ? "" : "  ") + std::string("(last known)");
            if (!info.empty()) {
                gui::Text(anchor.x, y, info, color, true);
                y += gui::kLineHeight;
            }
            if (settings.playerHealth && refs.health && refs.localHub && !localAlive)
                gui::Text(anchor.x, y, HealthText(hub), color, true);

            ++s.drawn;
        } catch (const rt::Error& e) {
            ++s.errors;
            s.lastError = e.what();
        }
    }
    NoteFrame(s, static_cast<int>(hubs.size()));
}

namespace {

std::unordered_map<int64_t, int64_t> g_categoryCache;
const MethodInfo* g_tryGetValue = nullptr;
bool g_syncValidated = false;
bool g_syncBroken = false;

int64_t CategoryOf(int64_t itemType, bool& known)
{
    known = false;
    if (!refs.itemCategory)
        return 0;
    if (auto it = g_categoryCache.find(itemType); it != g_categoryCache.end()) {
        known = true;
        return it->second;
    }

    Il2CppObject* dict = rt::Invoke(refs.itemLoaderAvailable, nullptr);
    if (!dict)
        return 0;
    if (!g_tryGetValue) {
        Il2CppClass* k = api.object_get_class(dict);
        void* iter = nullptr;
        while (const MethodInfo* m = api.class_get_methods(k, &iter)) {
            if (!std::strcmp(api.method_get_name(m), "TryGetValue") && api.method_get_param_count(m) == 2) {
                g_tryGetValue = m;
                break;
            }
        }
        if (!g_tryGetValue)
            throw rt::Error("Dictionary.TryGetValue not found");
    }

    int64_t key = itemType;
    Il2CppObject* item = nullptr;
    void* args[] = {&key, &item};
    int64_t category = 0;
    if (rt::InvokeValue<bool>(g_tryGetValue, dict, args) && item) {
        uint8_t bytes[8] = {};
        api.field_get_value(item, refs.itemBaseCategory, bytes);
        category = refs.categoryKind.Read(bytes);
        known = true;
    }
    g_categoryCache[itemType] = category;
    return category;
}

int CategoryIndex(int64_t category)
{
    for (size_t i = 0; i < refs.itemCategories.size(); ++i)
        if (refs.itemCategories[i].value == category)
            return static_cast<int>(i);
    return -1;
}

gui::Color CategoryColor(int64_t category, bool known)
{
    const char* name = known ? rt::EnumName(refs.itemCategories, category) : nullptr;
    if (!name) return {1, 1, 1, 1};
    if (!std::strcmp(name, "Keycard")) return {1, 0.85f, 0.2f, 1};
    if (!std::strcmp(name, "Medical")) return {0.3f, 1, 0.4f, 1};
    if (!std::strcmp(name, "Radio")) return {0.7f, 0.7f, 0.7f, 1};
    if (!std::strcmp(name, "Firearm")) return {1, 0.35f, 0.25f, 1};
    if (!std::strcmp(name, "Grenade")) return {1, 0.55f, 0.1f, 1};
    if (!std::strcmp(name, "SCPItem")) return {0.85f, 0.35f, 1, 1};
    if (!std::strcmp(name, "SpecialWeapon")) return {1, 0.2f, 0.6f, 1};
    if (!std::strcmp(name, "Ammo")) return {0.6f, 0.75f, 1, 1};
    if (!std::strcmp(name, "Armor")) return {0.35f, 0.65f, 1, 1};
    return {1, 1, 1, 1};
}

// Checks once, on real data, that the ItemId offset derived from metadata matches what il2cpp reads
// through the field API on a boxed copy.
void ValidateSyncLayout(const uint8_t* info)
{
    Il2CppObject* boxed = api.value_box(refs.pickupSyncInfo, const_cast<uint8_t*>(info));
    uint8_t viaApi[8] = {};
    api.field_get_value(boxed, refs.syncItemId, viaApi);
    if (std::memcmp(viaApi, info + refs.syncItemIdOffset, static_cast<size_t>(refs.itemTypeKind.size)) != 0) {
        g_syncBroken = true;
        SLM_LOG("PickupSyncInfo.ItemId offset check failed; item ESP disabled");
    }
    g_syncValidated = true;
}

} // namespace

void DrawItems(const View& v)
{
    Stats& s = itemStats;
    s.drawn = s.culled = s.errors = 0;
    if (s.disabled || !refs.items || !settings.itemEsp || g_syncBroken)
        return;

    std::vector<Il2CppObject*> pickups;
    if (!FindObjects(refs.itemPickupBase, kMaxPickups, pickups))
        return;

    uint32_t align = 0;
    const int32_t infoSize = api.class_value_size(refs.pickupSyncInfo, &align);
    for (Il2CppObject* p : pickups) {
        if (!Alive(p))
            continue;
        try {
            uint8_t info[64] = {};
            api.field_get_value(p, refs.pickupInfo, info);
            if (!g_syncValidated) {
                ValidateSyncLayout(info);
                if (g_syncBroken)
                    return;
            }
            const int64_t itemType = refs.itemTypeKind.Read(info + refs.syncItemIdOffset);
            if (itemType == refs.itemTypeNone)
                continue;
            if (refs.pickupFlagLocked >= 0 && !settings.itemShowLocked &&
                refs.syncFlagsOffset < static_cast<size_t>(infoSize) &&
                (info[refs.syncFlagsOffset] & static_cast<uint8_t>(refs.pickupFlagLocked)))
                continue;

            bool known = false;
            const int64_t category = CategoryOf(itemType, known);
            const int index = known ? CategoryIndex(category) : -1;
            if (index >= 0 && index < 32 && !settings.categoryEnabled[index])
                continue;

            const Vec3 pos = rt::InvokeValue<Vec3>(refs.pickupPosition, p);
            if (!Sane(pos))
                continue;
            const float distance = Distance(v.eye, pos);
            if (!(distance <= settings.itemMaxDistance))
                continue;
            Vec2 g;
            if (!Project(v, pos, g) || !IntersectsScreen({g.x, g.y, 0, 0}, v.w, v.h))
                continue;

            const char* name = rt::EnumName(refs.itemTypes, itemType);
            std::string label = name ? name : "#" + std::to_string(itemType);
            if (settings.itemDistance)
                label += " " + std::to_string(static_cast<int>(distance + 0.5f)) + "m";
            const gui::Color c = CategoryColor(category, known);
            gui::Dot(g, 3.0f, c);
            gui::Text(g.x, g.y + 3.0f, label, c, true);
            ++s.drawn;
        } catch (const rt::Error& e) {
            ++s.errors;
            s.lastError = e.what();
        }
    }
    NoteFrame(s, static_cast<int>(pickups.size()));
}

} // namespace slm::esp
