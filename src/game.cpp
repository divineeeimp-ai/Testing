#include "game.h"

#include "log.h"
#include "text.h"

#include <cstring>

namespace slm::game {

using il2cpp::api;
using rt::FindClass;
using rt::FindField;
using rt::FindMethod;

Refs refs;

int64_t EnumKind::Read(const void* p) const
{
    return EnumValueFromBytes(static_cast<const uint8_t*>(p), size, isSigned);
}

namespace {

void Problem(const std::string& text)
{
    refs.problems.push_back(text);
    SLM_LOG("PROBLEM: %s", text.c_str());
}

bool Enum(Il2CppClass* k, std::vector<rt::EnumEntry>& entries, EnumKind& kind, const char* what)
{
    if (!k || !rt::ReadEnum(k, entries) || !rt::EnumUnderlying(k, kind.size, kind.isSigned)) {
        Problem(std::string("enum ") + what + " unreadable");
        return false;
    }
    return true;
}

bool EnumConst(Il2CppClass* k, const char* name, int64_t& value, const char* what)
{
    std::vector<rt::EnumEntry> entries;
    if (k && rt::ReadEnum(k, entries) && rt::EnumValue(entries, name, value))
        return true;
    Problem(std::string(what) + "." + name + " not found");
    return false;
}

template <class... T>
bool All(T... p)
{
    return ((p != nullptr) && ...);
}

void ResolveUnity()
{
    Refs& r = refs;
    Il2CppClass* object = FindClass(r.core, "UnityEngine", "Object");
    r.unityObject = object;
    rt::SetUnityObjectClass(object);
    if (object && !api.class_get_field_from_name(object, "m_CachedPtr"))
        Problem("UnityEngine.Object.m_CachedPtr not found");

    r.findObjectsByType = FindMethod(object, "FindObjectsByType", {"System.Type", "UnityEngine.FindObjectsSortMode"});
    if (r.findObjectsByType)
        EnumConst(FindClass(r.core, "UnityEngine", "FindObjectsSortMode"), "None", r.sortModeNone, "FindObjectsSortMode");
    else
        r.findObjectsOfType = FindMethod(object, "FindObjectsOfType", {"System.Type"});
    if (!r.findObjectsByType && !r.findObjectsOfType)
        Problem("no FindObjectsByType / FindObjectsOfType: object lists unavailable");

    Il2CppClass* component = FindClass(r.core, "UnityEngine", "Component");
    Il2CppClass* transform = FindClass(r.core, "UnityEngine", "Transform");
    Il2CppClass* camera = FindClass(r.core, "UnityEngine", "Camera");
    Il2CppClass* screen = FindClass(r.core, "UnityEngine", "Screen");
    Il2CppClass* texture2d = FindClass(r.core, "UnityEngine", "Texture2D");
    r.componentGetTransform = FindMethod(component, "get_transform", {});
    r.transformGetPosition = FindMethod(transform, "get_position", {});
    r.transformTransformPoint = FindMethod(transform, "TransformPoint", {"UnityEngine.Vector3"});
    r.transformGetLossyScale = FindMethod(transform, "get_lossyScale", {});
    r.cameraGetMain = FindMethod(camera, "get_main", {});
    r.cameraWorldToCamera = FindMethod(camera, "get_worldToCameraMatrix", {});
    r.cameraProjection = FindMethod(camera, "get_projectionMatrix", {});
    r.cameraNearClip = FindMethod(camera, "get_nearClipPlane", {});
    r.cameraWorldToViewport = FindMethod(camera, "WorldToViewportPoint", {"UnityEngine.Vector3"});
    r.screenWidth = FindMethod(screen, "get_width", {});
    r.screenHeight = FindMethod(screen, "get_height", {});
    r.texWhite = FindMethod(texture2d, "get_whiteTexture", {});

    Il2CppClass* cc = FindClass(r.physics, "UnityEngine", "CharacterController");
    r.ccHeight = FindMethod(cc, "get_height", {});
    r.ccRadius = FindMethod(cc, "get_radius", {});
    r.ccCenter = FindMethod(cc, "get_center", {});

    Il2CppClass* gui = FindClass(r.imgui, "UnityEngine", "GUI");
    Il2CppClass* skin = FindClass(r.imgui, "UnityEngine", "GUISkin");
    Il2CppClass* style = FindClass(r.imgui, "UnityEngine", "GUIStyle");
    Il2CppClass* evt = FindClass(r.imgui, "UnityEngine", "Event");
    r.guiStyleClass = style;
    r.guiDrawTexture = FindMethod(gui, "DrawTexture", {"UnityEngine.Rect", "UnityEngine.Texture"});
    r.guiLabelStyle = FindMethod(gui, "Label", {"UnityEngine.Rect", "System.String", "UnityEngine.GUIStyle"});
    r.guiLabel = FindMethod(gui, "Label", {"UnityEngine.Rect", "System.String"});
    r.guiGetColor = FindMethod(gui, "get_color", {});
    r.guiSetColor = FindMethod(gui, "set_color", {"UnityEngine.Color"});
    r.guiGetSkin = FindMethod(gui, "get_skin", {});
    r.skinGetLabel = FindMethod(skin, "get_label", {});
    r.styleCtorCopy = FindMethod(style, ".ctor", {"UnityEngine.GUIStyle"});
    r.styleSetAlignment = FindMethod(style, "set_alignment", {"UnityEngine.TextAnchor"});
    r.styleSetFontSize = FindMethod(style, "set_fontSize", {"System.Int32"});
    r.styleSetRichText = FindMethod(style, "set_richText", {"System.Boolean"});
    r.guiGetMatrix = FindMethod(gui, "get_matrix", {});
    r.guiSetMatrix = FindMethod(gui, "set_matrix", {"UnityEngine.Matrix4x4"});
    r.guiRotateAroundPivot = FindMethod(FindClass(r.imgui, "UnityEngine", "GUIUtility"), "RotateAroundPivot",
                                        {"System.Single", "UnityEngine.Vector2"});
    r.eventGetCurrent = FindMethod(evt, "get_current", {});
    r.eventGetType = FindMethod(evt, "get_type", {});

    Il2CppClass* eventType = FindClass(r.imgui, "UnityEngine", "EventType");
    std::vector<rt::EnumEntry> eventTypes;
    if (eventType && Enum(eventType, eventTypes, r.eventTypeKind, "EventType"))
        rt::EnumValue(eventTypes, "Repaint", r.eventTypeRepaint);

    Il2CppClass* anchor = api.class_from_name(r.core, "UnityEngine", "TextAnchor");
    if (!anchor) {
        if (const Il2CppImage* text = rt::FindImage("UnityEngine.TextRenderingModule.dll"))
            anchor = api.class_from_name(text, "UnityEngine", "TextAnchor");
    }
    if (anchor) {
        EnumConst(anchor, "UpperLeft", r.textAnchorUpperLeft, "TextAnchor");
        EnumConst(anchor, "UpperCenter", r.textAnchorUpperCenter, "TextAnchor");
    }

    r.drawing = All(r.guiDrawTexture, r.texWhite, r.guiSetColor, r.guiGetColor, r.screenWidth, r.screenHeight) &&
                (r.guiLabelStyle || r.guiLabel) && (r.findObjectsByType || r.findObjectsOfType);
    if (!r.drawing)
        Problem("IMGUI drawing unavailable (see missing methods above)");
    r.camera = All(r.cameraGetMain, r.cameraWorldToCamera, r.cameraProjection, r.cameraNearClip,
                   r.componentGetTransform, r.transformGetPosition);
    if (!r.camera)
        Problem("camera projection unavailable");
}

void ResolvePlayers()
{
    Refs& r = refs;
    Il2CppClass* hub = FindClass(r.acs, "", "ReferenceHub");
    r.referenceHub = hub;
    r.hubRoleManager = FindField(hub, "roleManager");
    r.hubPlayerStats = FindField(hub, "playerStats");
    r.hubNicknameSync = FindField(hub, "nicknameSync");
    r.hubTryGetLocal = FindMethod(hub, "TryGetLocalHub", {"ReferenceHub&"});

    Il2CppClass* roleManager = FindClass(r.acs, "PlayerRoles", "PlayerRoleManager");
    Il2CppClass* role = FindClass(r.acs, "PlayerRoles", "PlayerRoleBase");
    r.roleManagerCurrentRole = FindMethod(roleManager, "get_CurrentRole", {});
    r.roleTeam = FindMethod(role, "get_Team", {});
    r.roleColor = FindMethod(role, "get_RoleColor", {});
    r.roleName = FindMethod(role, "get_RoleName", {});

    Il2CppClass* team = FindClass(r.acs, "PlayerRoles", "Team");
    if (Enum(team, r.teams, r.teamKind, "Team") && !rt::EnumValue(r.teams, "Dead", r.teamDead))
        Problem("Team.Dead not found");

    r.fpcRoleInterface = FindClass(r.acs, "PlayerRoles.FirstPersonControl", "IFpcRole");
    r.fpcRoleModule = FindMethod(r.fpcRoleInterface, "get_FpcModule", {});
    Il2CppClass* fpm = FindClass(r.acs, "PlayerRoles.FirstPersonControl", "FirstPersonMovementModule");
    r.fpmPosition = FindMethod(fpm, "get_Position", {});
    r.fpmModuleReady = FindMethod(fpm, "get_ModuleReady", {});
    r.fpmCharControllerSet = FindMethod(fpm, "get_CharControllerSet", {});
    r.fpmCharController = FindMethod(fpm, "get_CharController", {});
    r.fpmMotor = FindMethod(fpm, "get_Motor", {});
    r.fpmCharacterModel = FindMethod(fpm, "get_CharacterModelInstance", {});
    Il2CppClass* motor = FindClass(r.acs, "PlayerRoles.FirstPersonControl", "FpcMotor");
    r.motorIsInvisible = FindMethod(motor, "get_IsInvisible", {});

    Il2CppClass* model = FindClass(r.acs, "PlayerRoles.FirstPersonControl.Thirdperson", "CharacterModel");
    r.modelHitboxes = FindMethod(model, "get_Hitboxes", {});
    Il2CppClass* hitbox = FindClass(r.acs, "", "HitboxIdentity");
    r.hitboxType = FindMethod(hitbox, "get_HitboxType", {});
    r.hitboxCenter = FindMethod(hitbox, "get_CenterOfMass", {});
    r.hitboxIsEnemy = FindMethod(hitbox, "IsEnemy", {"PlayerRoles.Team", "PlayerRoles.Team"});
    Il2CppClass* hitboxType = FindClass(r.acs, "", "HitboxType");
    std::vector<rt::EnumEntry> hitboxTypes;
    if (Enum(hitboxType, hitboxTypes, r.hitboxKind, "HitboxType") && !rt::EnumValue(hitboxTypes, "Headshot", r.hitboxHeadshot))
        Problem("HitboxType.Headshot not found");

    Il2CppClass* stats = FindClass(r.acs, "PlayerStatsSystem", "PlayerStats");
    Il2CppClass* statBase = FindClass(r.acs, "PlayerStatsSystem", "StatBase");
    r.statsModules = FindMethod(stats, "get_StatModules", {});
    r.statCur = FindMethod(statBase, "get_CurValue", {});
    r.statMax = FindMethod(statBase, "get_MaxValue", {});
    r.healthStat = FindClass(r.acs, "PlayerStatsSystem", "HealthStat");
    r.humeShieldStat = FindClass(r.acs, "PlayerStatsSystem", "HumeShieldStat");

    Il2CppClass* nick = FindClass(r.acs, "", "NicknameSync");
    r.nickDisplayName = FindMethod(nick, "get_DisplayName", {});

    r.players = r.drawing && r.camera &&
                All(r.hubRoleManager, r.hubNicknameSync, r.roleManagerCurrentRole, r.roleTeam, r.roleColor, r.roleName,
                    r.fpcRoleInterface, r.fpcRoleModule, r.fpmPosition, r.fpmModuleReady, r.fpmMotor,
                    r.motorIsInvisible, r.nickDisplayName) &&
                r.teamDead >= 0;
    r.localHub = r.players && r.hubTryGetLocal;
    r.teamFilter = r.localHub && r.hitboxIsEnemy;
    if (r.players && !r.localHub)
        Problem("local player unknown: own body skipped by distance, teammates not filtered");
    else if (r.players && !r.teamFilter)
        Problem("teammate filter unavailable");
    r.box = r.players && All(r.fpmCharControllerSet, r.fpmCharController, r.ccHeight, r.ccRadius, r.ccCenter,
                             r.componentGetTransform, r.transformTransformPoint, r.transformGetLossyScale);
    r.head = r.players && All(r.fpmCharacterModel, r.modelHitboxes, r.hitboxType, r.hitboxCenter) && r.hitboxHeadshot >= 0;
    r.health = r.players && All(r.hubPlayerStats, r.statsModules, r.statCur, r.statMax, r.healthStat);
    if (!r.players)
        Problem("player ESP disabled");
    else {
        if (!r.box) Problem("player boxes disabled");
        if (!r.head) Problem("head dot disabled");
        if (!r.health) Problem("health readout disabled");
    }
}

void ResolveItems()
{
    Refs& r = refs;
    Il2CppClass* pickup = FindClass(r.acs, "InventorySystem.Items.Pickups", "ItemPickupBase");
    r.itemPickupBase = pickup;
    r.pickupInfo = FindField(pickup, "Info");
    r.pickupPosition = FindMethod(pickup, "get_Position", {});
    r.pickupSyncInfo = FindClass(r.acs, "InventorySystem.Items.Pickups", "PickupSyncInfo");
    r.syncItemId = r.pickupSyncInfo ? api.class_get_field_from_name(r.pickupSyncInfo, "ItemId") : nullptr;
    r.syncFlags = r.pickupSyncInfo ? api.class_get_field_from_name(r.pickupSyncInfo, "_flags") : nullptr;

    Il2CppClass* itemType = FindClass(r.acs, "", "ItemType");
    if (Enum(itemType, r.itemTypes, r.itemTypeKind, "ItemType") && !rt::EnumValue(r.itemTypes, "None", r.itemTypeNone))
        Problem("ItemType.None not found");

    // Field offsets of a value type are reported relative to the boxed object; the base was measured
    // from UnityEngine.Vector3 in ValidateLayouts().
    const size_t base = rt::ValueFieldBase();
    uint32_t align = 0;
    const int32_t syncSize = r.pickupSyncInfo ? api.class_value_size(r.pickupSyncInfo, &align) : 0;
    bool syncLayout = r.syncItemId && base != static_cast<size_t>(-1) && syncSize > 0 && syncSize <= 64;
    if (syncLayout) {
        r.syncItemIdOffset = api.field_get_offset(r.syncItemId) - base;
        syncLayout = r.syncItemIdOffset + r.itemTypeKind.size <= static_cast<size_t>(syncSize) &&
                     rt::TypeName(api.field_get_type(r.syncItemId)) == "ItemType";
    }
    if (!syncLayout)
        Problem("PickupSyncInfo.ItemId layout not usable");

    if (r.syncFlags) {
        r.syncFlagsOffset = api.field_get_offset(r.syncFlags) - base;
        Il2CppClass* flags = rt::FindNested(r.pickupSyncInfo, "PickupFlags");
        if (!flags || !EnumConst(flags, "Locked", r.pickupFlagLocked, "PickupFlags") ||
            r.syncFlagsOffset >= static_cast<size_t>(syncSize))
            r.pickupFlagLocked = -1;
    }

    Il2CppClass* loader = FindClass(r.acs, "InventorySystem", "InventoryItemLoader");
    r.itemLoaderAvailable = FindMethod(loader, "get_AvailableItems", {});
    Il2CppClass* itemBase = FindClass(r.acs, "InventorySystem.Items", "ItemBase");
    r.itemBaseCategory = FindField(itemBase, "Category");
    Il2CppClass* category = FindClass(r.acs, "", "ItemCategory");
    r.itemCategory = r.itemLoaderAvailable && r.itemBaseCategory &&
                     Enum(category, r.itemCategories, r.categoryKind, "ItemCategory");

    r.items = r.drawing && r.camera && All(pickup, r.pickupInfo, r.pickupPosition, r.pickupSyncInfo) && syncLayout &&
              !r.itemTypes.empty();
    if (!r.items)
        Problem("item ESP disabled");
    else if (!r.itemCategory)
        Problem("item categories unavailable (items shown uncategorised)");
}

void ResolveMisc()
{
    Refs& r = refs;
    Il2CppClass* mcc = FindClass(r.acs, "", "MainCameraController");
    r.mccLastPosition = FindMethod(mcc, "get_LastPosition", {});
    Il2CppClass* sum = FindClass(r.acs, "", "StaticUnityMethods");
    r.staticUnityMethods = sum;
    r.sumOnGui = FindMethod(sum, "OnGUI", {});
    r.sumUpdate = FindMethod(sum, "Update", {});
    r.sumLateUpdate = FindMethod(sum, "LateUpdate", {});
    r.sumFixedUpdate = FindMethod(sum, "FixedUpdate", {});
    Il2CppClass* version = FindClass(r.acs, "GameCore", "Version");
    r.versionString = version ? api.class_get_field_from_name(version, "VersionString") : nullptr;
}

} // namespace

bool Resolve()
{
    Refs& r = refs;
    r.acs = rt::FindImage("Assembly-CSharp.dll");
    r.core = rt::FindImage("UnityEngine.CoreModule.dll");
    r.imgui = rt::FindImage("UnityEngine.IMGUIModule.dll");
    r.physics = rt::FindImage("UnityEngine.PhysicsModule.dll");
    if (!r.acs || !r.core || !r.imgui) {
        Problem("required images missing (Assembly-CSharp / UnityEngine.CoreModule / UnityEngine.IMGUIModule)");
        return false;
    }
    if (!r.physics)
        Problem("UnityEngine.PhysicsModule.dll missing: boxes disabled");

    std::string layout;
    if (!rt::ValidateLayouts(r.core, layout)) {
        Problem("Unity struct layout check failed: " + layout);
        return false;
    }
    SLM_LOG("value-type field base offset: %zu", rt::ValueFieldBase());

    ResolveUnity();
    ResolvePlayers();
    ResolveItems();
    ResolveMisc();
    SLM_LOG("availability: drawing=%d camera=%d players=%d box=%d head=%d health=%d items=%d categories=%d",
            r.drawing, r.camera, r.players, r.box, r.head, r.health, r.items, r.itemCategory);
    return r.drawing && r.sumOnGui;
}

} // namespace slm::game
