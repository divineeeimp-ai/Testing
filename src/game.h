#pragma once
// Every game and Unity member SLMenu uses, resolved by name at startup.
// Names were read from Assembly-CSharp of SCP:SL 14.2.8 (dedicated-server build, NuGet ExMod.Exiled 9.15.2)
// and Unity's engine API; the client build is IL2CPP (Unity 6000.0.43f1, build tag release-vanilla-efcc14c8),
// so each name is confirmed against the running client here, and a missing one disables only its feature.

#include "il2cpp_api.h"
#include "runtime.h"

#include <string>
#include <vector>

namespace slm::game {

struct EnumKind {
    int size = 0;
    bool isSigned = false;
    int64_t Read(const void* p) const;
};

struct Refs {
    EnumKind teamKind, hitboxKind, itemTypeKind, categoryKind, eventTypeKind;

    // images
    const Il2CppImage* acs = nullptr;  // Assembly-CSharp.dll
    const Il2CppImage* core = nullptr; // UnityEngine.CoreModule.dll
    const Il2CppImage* imgui = nullptr;
    const Il2CppImage* physics = nullptr;

    // ---- Unity core
    Il2CppClass* unityObject = nullptr;
    const MethodInfo* findObjectsByType = nullptr;  // Object.FindObjectsByType(Type, FindObjectsSortMode)
    const MethodInfo* findObjectsOfType = nullptr;  // Object.FindObjectsOfType(Type) (fallback)
    int64_t sortModeNone = 0;
    const MethodInfo* componentGetTransform = nullptr;
    const MethodInfo* transformGetPosition = nullptr;
    const MethodInfo* transformTransformPoint = nullptr;
    const MethodInfo* transformGetLossyScale = nullptr;
    const MethodInfo* cameraGetMain = nullptr;
    const MethodInfo* cameraWorldToCamera = nullptr;
    const MethodInfo* cameraProjection = nullptr;
    const MethodInfo* cameraNearClip = nullptr;
    const MethodInfo* cameraWorldToViewport = nullptr; // optional cross-check
    const MethodInfo* screenWidth = nullptr;
    const MethodInfo* screenHeight = nullptr;
    const MethodInfo* texWhite = nullptr; // Texture2D.whiteTexture

    // ---- physics
    const MethodInfo* ccHeight = nullptr;
    const MethodInfo* ccRadius = nullptr;
    const MethodInfo* ccCenter = nullptr;

    // ---- IMGUI
    const MethodInfo* guiDrawTexture = nullptr;  // GUI.DrawTexture(Rect, Texture)
    const MethodInfo* guiLabelStyle = nullptr;   // GUI.Label(Rect, String, GUIStyle)
    const MethodInfo* guiLabel = nullptr;        // GUI.Label(Rect, String)
    const MethodInfo* guiGetColor = nullptr;
    const MethodInfo* guiSetColor = nullptr;
    const MethodInfo* guiGetSkin = nullptr;
    const MethodInfo* skinGetLabel = nullptr;
    Il2CppClass* guiStyleClass = nullptr;
    const MethodInfo* styleCtorCopy = nullptr;   // GUIStyle(GUIStyle)
    const MethodInfo* styleSetAlignment = nullptr;
    const MethodInfo* styleSetFontSize = nullptr;
    const MethodInfo* styleSetRichText = nullptr;
    const MethodInfo* guiGetMatrix = nullptr;          // optional: snaplines
    const MethodInfo* guiSetMatrix = nullptr;
    const MethodInfo* guiRotateAroundPivot = nullptr;  // GUIUtility.RotateAroundPivot(Single, Vector2)
    const MethodInfo* eventGetCurrent = nullptr;
    const MethodInfo* eventGetType = nullptr;
    int64_t eventTypeRepaint = -1;
    int64_t textAnchorUpperLeft = -1;
    int64_t textAnchorUpperCenter = -1;

    // ---- game: players
    Il2CppClass* referenceHub = nullptr;
    FieldInfo* hubRoleManager = nullptr;
    FieldInfo* hubPlayerStats = nullptr;
    FieldInfo* hubNicknameSync = nullptr;
    const MethodInfo* hubTryGetLocal = nullptr;
    const MethodInfo* roleManagerCurrentRole = nullptr;
    const MethodInfo* roleTeam = nullptr;      // virtual
    const MethodInfo* roleColor = nullptr;     // virtual
    const MethodInfo* roleName = nullptr;
    Il2CppClass* fpcRoleInterface = nullptr;
    const MethodInfo* fpcRoleModule = nullptr; // interface
    const MethodInfo* fpmPosition = nullptr;
    const MethodInfo* fpmModuleReady = nullptr;
    const MethodInfo* fpmCharControllerSet = nullptr;
    const MethodInfo* fpmCharController = nullptr;
    const MethodInfo* fpmMotor = nullptr;
    const MethodInfo* fpmCharacterModel = nullptr;
    const MethodInfo* motorIsInvisible = nullptr;
    const MethodInfo* modelHitboxes = nullptr;
    const MethodInfo* hitboxType = nullptr;
    const MethodInfo* hitboxCenter = nullptr;
    const MethodInfo* hitboxIsEnemy = nullptr; // static IsEnemy(Team, Team)
    int64_t hitboxHeadshot = -1;
    const MethodInfo* statsModules = nullptr;
    const MethodInfo* statCur = nullptr; // virtual
    const MethodInfo* statMax = nullptr; // virtual
    Il2CppClass* healthStat = nullptr;
    Il2CppClass* humeShieldStat = nullptr;
    const MethodInfo* nickDisplayName = nullptr;
    std::vector<rt::EnumEntry> teams;
    int64_t teamDead = -1;

    // ---- game: items
    Il2CppClass* itemPickupBase = nullptr;
    FieldInfo* pickupInfo = nullptr;
    Il2CppClass* pickupSyncInfo = nullptr;
    FieldInfo* syncItemId = nullptr;
    FieldInfo* syncFlags = nullptr;
    size_t syncItemIdOffset = 0; // inside the unboxed struct
    size_t syncFlagsOffset = 0;
    int64_t pickupFlagLocked = -1;
    const MethodInfo* pickupPosition = nullptr;
    const MethodInfo* itemLoaderAvailable = nullptr; // InventoryItemLoader.AvailableItems (static)
    FieldInfo* itemBaseCategory = nullptr;
    std::vector<rt::EnumEntry> itemTypes;
    std::vector<rt::EnumEntry> itemCategories;
    int64_t itemTypeNone = -1;

    // ---- game: misc
    const MethodInfo* mccLastPosition = nullptr; // MainCameraController.LastPosition (cross-check)
    Il2CppClass* staticUnityMethods = nullptr;
    const MethodInfo* sumOnGui = nullptr;
    const MethodInfo* sumUpdate = nullptr;
    const MethodInfo* sumLateUpdate = nullptr;
    const MethodInfo* sumFixedUpdate = nullptr;
    FieldInfo* versionString = nullptr; // GameCore.Version.VersionString (static)

    // ---- availability
    bool drawing = false;   // can draw rectangles and text at all
    bool camera = false;
    bool players = false;
    bool localHub = false;   // ReferenceHub.TryGetLocalHub usable
    bool teamFilter = false; // HitboxIdentity.IsEnemy(Team, Team) usable
    bool health = false;
    bool head = false;
    bool box = false;
    bool items = false;
    bool itemCategory = false;
    std::vector<std::string> problems;
};

extern Refs refs;

// Must run on a thread attached to the il2cpp domain.
bool Resolve();

} // namespace slm::game
