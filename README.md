# SLMenu: internal Mono menu for the SCP: Secret Laboratory client

SLMenu is an internal C# DLL. It is injected into the running game client (Mono runtime) and draws a Unity IMGUI menu with:

- **Player ESP:** box, head dot, name, role, distance, health (only while you are dead or spectating), snaplines, teammate filter, and an optional "last known position" view for players whose position the server is withholding.
- **Item ESP:** every pickup on the map with its `ItemType` name and distance, coloured and filtered by the game's own `ItemCategory`, plus a max-distance slider.

**Insert** toggles the menu. While it is open, the cursor is unlocked and movement is locked through the game's own `CursorManager`. The Info tab shows the build check results and has an **Unload** button.

## How the game data was obtained

| What | Source | Status |
|---|---|---|
| Class and member names | `Assembly-CSharp-Publicized.dll` from NuGet `ExMod.Exiled` 9.15.2, decompiled with ILSpy 8.2 | measured |
| Game version of that assembly | `GameCore.Version`: **14.2.8, Release** | measured |
| Which used members are definitely public | Cross-checked against `LabApi.dll` (Northwood, NuGet `Northwood.LabAPI`), which is compiled against the non-publicized assembly | partly measured (see below) |
| Your client build | `C:\Program Files (x86)\Steam\...\SCP Secret Laboratory` was **not** reachable from the build machine | **not measured** |

The reference assembly is the **dedicated-server** build of the same codebase. SLMenu uses **no raw offsets or signatures**: all access goes through managed type and member names. A different game version can therefore only break by renaming or removing members, and that is caught in two places:

1. **Compile time.** The project compiles against *your* `SCPSL_Data\Managed\Assembly-CSharp.dll`, without a publicizer. If a member is missing or non-public in your build, the compiler reports it (CS0117 or CS0122) by name.
2. **Runtime.** `GameBuild.Validate()` checks every required type and member by reflection. It turns off only the affected feature and lists the problem on the Info tab and in `Player.log`.

Members whose public visibility is **not** proven by LabApi (the publicized reference hides the original modifiers): `ReferenceHub.TryGetLocalHub`, `PlayerRoleBase.RoleColor`, `FirstPersonMovementModule.{ModuleReady, CharController, CharControllerSet, CharacterModelInstance}`, `FpcMotor.IsInvisible`, `CharacterModel.Hitboxes`, `HitboxIdentity.{HitboxType, CenterOfMass, IsEnemy}`, `MainCameraController.{CurrentCamera, LastPosition}`, `StaticUnityMethods.OnUpdate`, `GameCore.Version.*`, and `CursorManagement.*`. If one of these is not public in your build, step 2 of the build below fails and names it.

## What the client can actually see (measured from the 14.2.8 code)

- **Player positions are culled by the server.** `FpcServerPositionDistributor` only sends targets that pass the receiver's `FpcVisibilityController`: within √1300 ≈ 36 m inside the facility, within √4900 = 70 m on Surface, and not hidden by effects such as SCP-268. For every player missing from a packet, the client sets `FpcModule.Motor.IsInvisible = true` and keeps the last received position. SLMenu hides those players by default. With "Show last known position" on, it draws them faded with a `(last known)` tag. The Players tab shows how many players are currently withheld.
- **Other players' health is not sent to living players.** `HealthStat` and `HumeShieldStat` use `SyncMode.PrivateAndSpectators`, so real values arrive only while your own team is `Team.Dead`. Health text is drawn only then.
- **Pickups are not culled.** The game's `CustomInterestManagement` makes every networked object visible to every ready client, so item ESP sees the whole map. The game keeps no static pickup list: SLMenu subscribes to `ItemPickupBase.OnPickupAdded` and `OnPickupDestroyed` and seeds the list once with `FindObjectsOfType<ItemPickupBase>()`.

## Requirements

- Windows, with SCP: Secret Laboratory installed and using the **Mono** backend. Check it first:
  ```bash
  powershell -ExecutionPolicy Bypass -File tools\Check-GameBuild.ps1
  ```
  The script reports MONO or IL2CPP and prints the sizes and SHA-256 hashes of the DLLs SLMenu compiles against. The build also refuses to run if `GameAssembly.dll` exists (that file means an IL2CPP build).
- .NET SDK 6 or newer. Targeting `net48` needs no Visual Studio: the SDK pulls `Microsoft.NETFramework.ReferenceAssemblies` automatically.
- A Mono injector. Examples below use SharpMonoInjector (`smi.exe`).

## Build

```bash
dotnet build SLMenu.sln -c Release
```

If the game is installed somewhere else:

```bash
dotnet build SLMenu.sln -c Release -p:GameDir="D:\SteamLibrary\steamapps\common\SCP Secret Laboratory"
```

Output: `src\SLMenu\bin\Release\SLMenu.dll`. Game assemblies are referenced with `Private=false`, so nothing from the game is copied to the output.

## Inject and eject

The process name is `SCPSL`: Unity names the data folder `<exe>_Data`, and the folder is `SCPSL_Data`.

```bash
smi.exe inject -p SCPSL -a src\SLMenu\bin\Release\SLMenu.dll -n SLMenu -c Loader -m Load
```

```bash
smi.exe eject -p SCPSL -a <address printed by inject> -n SLMenu -c Loader -m Unload
```

Injectors call `Load` and `Unload` on their own thread. SLMenu therefore defers object creation and destruction to the game's `StaticUnityMethods.OnUpdate` event, which runs on the Unity main thread. Mono cannot unload an assembly from the root domain, so the code stays in memory after `Unload`. The menu object, event subscriptions, cursor override and textures are all released.

## Layout

| File | Role |
|---|---|
| `src/SLMenu/Loader.cs` | Injector entry points `SLMenu.Loader.Load` and `Unload`, deferred to the main thread |
| `src/SLMenu/MenuBehaviour.cs` | Menu window, hotkey, per-feature error isolation, Info tab |
| `src/SLMenu/GameBuild.cs` | Reference build (14.2.8), runtime version, Mono and member validation |
| `src/SLMenu/CursorUnlock.cs` | `ICursorOverride` registered with the game's `CursorManager` |
| `src/SLMenu/Settings.cs` | Menu options (kept in memory only) |
| `src/SLMenu/Esp/PlayerEsp.cs` | Player collection and drawing |
| `src/SLMenu/Esp/PickupTracker.cs` | Live set of `ItemPickupBase` instances |
| `src/SLMenu/Esp/ItemEsp.cs` | Pickup collection and drawing |
| `src/SLMenu/Render/ScreenMath.cs` | Pure viewport-to-IMGUI and box math (checked outside the game) |
| `src/SLMenu/Render/ViewCamera.cs` | Camera selection and world-to-GUI projection |
| `src/SLMenu/Render/Draw.cs` | IMGUI primitives: rectangles, outlines, lines, dots, text |
| `tools/Check-GameBuild.ps1` | Mono/IL2CPP check and DLL fingerprints |

## Coordinates and rendering

- Unity world space: left-handed, Y up, metres.
- Projection uses `Camera.WorldToViewportPoint` and is converted to IMGUI pixels with the origin top-left. Points behind the near plane or with non-finite values are rejected.
- Everything is collected and drawn in `OnGUI` during `EventType.Repaint`, which runs after that frame's `LateUpdate`.
- Boxes come from each player's live `CharacterController` (`center`, `height` and `radius`; the game changes height and center while crouching). The head dot is the `HitboxType.Headshot` hitbox's `CenterOfMass`.
- Camera: `Camera.main`, falling back to the `Camera` under `MainCameraController.CurrentCamera`. Which `Camera` component the game renders the world through is prefab data that is not in the assembly. The Info tab shows the chosen camera and its distance from `MainCameraController.LastPosition`. That distance should be about 0 m.

## Verification status

- Built in a Linux container against the 14.2.8 reference assembly, Unity 2021.3.33 reference modules (NuGet `UnityEngine.Modules`), and compile-only stand-ins for `Mirror.dll` and `Pooling.dll`. The output DLL references no members of either assembly. Result: 0 warnings, 0 errors.
- The `ScreenMath` checks pass (19 cases).
- `Check-GameBuild.ps1` was run under PowerShell 7.4 on mock Mono, IL2CPP and missing folders.
- **Not built against your client DLLs and not run in the game.** First in-game checks: open the Info tab and confirm that the version is 14.2.8 or the problems list is empty, that the camera offset is about 0 m, and that boxes line up with player models, including while crouching.
