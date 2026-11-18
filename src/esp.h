#pragma once
// Player and item ESP. Called from the hooked StaticUnityMethods.OnGUI during EventType.Repaint.
//
// What the server sends this client (measured in the 14.2.8 assembly):
//  - Positions: FpcServerPositionDistributor skips targets the receiver's FpcVisibilityController rejects
//    (beyond sqrt(1300) ~ 36 m inside the facility, sqrt(4900) = 70 m on Surface, SCP-268, ...). On receive,
//    FpcPositionMessage sets FpcModule.Motor.IsInvisible = true for every player not in the packet; their
//    Position is then the last one received.
//  - HealthStat / HumeShieldStat use SyncMode.PrivateAndSpectators: other players' values arrive only while
//    the local player is dead (Team.Dead).
//  - Pickups: CustomInterestManagement makes every networked object visible to every ready client.

#include "math.h"
#include "runtime.h"

#include <string>

namespace slm::esp {

struct View {
    Il2CppObject* camera = nullptr;
    Mat4 view{}, proj{};
    float nearClip = 0.0f;
    Vec3 eye{};
    float w = 0.0f, h = 0.0f;
};

struct Stats {
    int drawn = 0;
    int culled = 0;
    int errors = 0;
    int failingFrames = 0;
    bool disabled = false;
    std::string lastError;
};

struct Diagnostics {
    float projectionErrorPx = -1.0f; // our projection vs Camera.WorldToViewportPoint
    float cameraOffset = -1.0f;      // Camera.main position vs MainCameraController.LastPosition
};

extern Stats playerStats;
extern Stats itemStats;
extern Diagnostics diagnostics;

bool BuildView(View& v);
bool Project(const View& v, Vec3 world, Vec2& gui);
void DrawPlayers(const View& v);
void DrawItems(const View& v);

} // namespace slm::esp
