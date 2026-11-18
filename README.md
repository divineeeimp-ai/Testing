# SLMenu: internal IL2CPP menu for the SCP: Secret Laboratory client

A native x64 DLL (C++20) for the IL2CPP game client, with **player ESP** and **item ESP**.

- **Player ESP:** box, head dot, name, role, distance, health (only while you are dead or spectating), snaplines, a teammate filter, and an optional faded "last known position" for players whose position the server is withholding.
- **Item ESP:** every pickup with its `ItemType` name and distance, coloured and filtered by `ItemCategory`, with locked pickups optional and a distance limit.

Everything is drawn through **Unity's own IMGUI** from a hook on the game's `StaticUnityMethods.OnGUI`. That makes it independent of the graphics API; this install renders with Vulkan.

## How it reaches game data: no offsets

| Need | How | Source |
|---|---|---|
| Classes, methods, fields | `il2cpp_class_from_name`, `il2cpp_class_get_methods` and parameter type names, `il2cpp_class_get_field_from_name` | your running client |
| Calls into game and Unity code | `il2cpp_runtime_invoke`; managed exceptions are captured, not propagated | your running client |
| Abstract, virtual and interface members (`Team`, `RoleColor`, `FpcModule`, `CurValue`) | `il2cpp_object_get_virtual_method` | your running client |
| Enum values (`Team`, `ItemType`, `ItemCategory`, `HitboxType`, `PickupFlags`, `EventType`) | read from the enum's literal fields at runtime | your running client |
| Unity struct layouts (`Vector2`, `Vector3`, `Rect`, `Color`, `Matrix4x4`) | checked against their field offsets at startup | your running client |
| `PickupSyncInfo.ItemId` position inside the struct | field offset from metadata, then cross-checked once against `il2cpp_field_get_value` on a boxed copy | your running client |
| Hook target | `MethodInfo::methodPointer` of `StaticUnityMethods.OnGUI`; must lie inside `GameAssembly.dll` and must not be shared with `Update`, `LateUpdate` or `FixedUpdate` (identical-code folding) | your running client |
| Player and pickup lists | `Object.FindObjectsByType(Type, None)` each frame; nothing managed is held across frames except one `GUIStyle`, kept alive with a GC handle | your running client |

Names come from Assembly-CSharp **14.2.8** (the dedicated-server build of the same code, in NuGet `ExMod.Exiled` 9.15.2). Your client is a different build (`release-vanilla-efcc14c8`). Anything renamed there is reported, and only the feature that needs it is switched off.

## Measured on your install (from your PowerShell output)

- **Scripting backend:** IL2CPP. `GameAssembly.dll` is 71,179,616 bytes; there is no `Managed` folder.
- **Engine:** Unity **6000.0.43f1**. Graphics: **Vulkan** (`Player.log`: `Forcing GfxDevice: Vulkan`).
- **`global-metadata.dat`:** the magic is `AF 1B B1 FA`, so it is not encrypted, and class names are readable. The header claims **version 23**, which is unusual for Unity 6. SLMenu never parses this file, so the value does not matter to it.
- **Exports:** the `il2cpp_*` export names are present in `GameAssembly.dll`. SLMenu resolves each one with `GetProcAddress` at startup.

## What the server sends this client (from the 14.2.8 code)

- **Positions:** `FpcServerPositionDistributor` only sends players who pass your `FpcVisibilityController`: about 36 m (√1300) inside the facility, 70 m (√4900) on Surface, and not hidden by effects such as SCP-268. For each player missing from a packet, the client sets `Motor.IsInvisible` and keeps the last position it received. Such players are hidden by default; enable "Show last known" to see them faded.
- **Health:** `HealthStat` and `HumeShieldStat` reach you only while your own team is `Dead`. Health is drawn only then.
- **Pickups:** the game's `CustomInterestManagement` sends every networked object to every client, so item ESP covers the whole map.

## Build (Windows, Visual Studio 2022 or Build Tools with "Desktop development with C++")

Open **Developer PowerShell for VS 2022** in the repository folder:

```bash
cmake -S . -B build -A x64
```

```bash
cmake --build build --config Release
```

Output: `build\Release\SLMenu.dll` (MinHook is linked in statically, along with the static C runtime) and `build\Release\SLInject.exe`.

## Use

1. Start SCP:SL and join a server, or wait at the main menu.
2. Inject:
   ```bash
   build\Release\SLInject.exe build\Release\SLMenu.dll
   ```
3. The menu opens in the top-left corner.

| Key | Action |
|---|---|
| Insert | show or hide the menu |
| Up / Down | select |
| Left / Right | change a slider (and flip a toggle) |
| Enter | flip a toggle |
| End | unload (restores `OnGUI`; the DLL stays mapped) |

Keys only act while the game window has focus. They are not consumed, so the game also sees them. Change them in `src/settings.h` if they collide with your binds.

The log is `SLMenu.log`, written next to `SLMenu.dll`. It lists every resolved and missing member, each "candidate" overload when a lookup fails, the hook address as `GameAssembly+0x...`, and the game version string.

## Unload

End (or the menu's Unload entry) disables the hook, which restores `OnGUI`'s original bytes. The DLL and its trampoline stay loaded on purpose: a main-thread call that entered the detour just before the patch could still be returning through them. The module is pinned, so the injector cannot free it either.

## Layout

| File | Role |
|---|---|
| `src/dllmain.cpp` | init thread, `OnGUI` hook, per-frame guard, unload |
| `src/il2cpp_api.*` | `il2cpp_*` export table |
| `src/runtime.*` | lookup by name, invoke, enums, arrays, strings, layout validation |
| `src/game.*` | every game and Unity member used, resolved and reported per feature |
| `src/esp.*` | player and item ESP, camera matrices, diagnostics |
| `src/gui.*` | IMGUI drawing (rectangles, outlines, lines, text) |
| `src/menu.*` | keyboard menu and info section |
| `src/math.h`, `src/text.h` | pure projection, box, UTF-16 and enum helpers |
| `src/settings.h` | options and keys |
| `injector/injector.cpp` | `LoadLibraryW` injector with process and module checks |
| `tools/Check-GameBuild.ps1` | backend check and file fingerprints |
| `third_party/minhook` | MinHook (BSD-2-Clause), commit 8af6b4a |

## Coordinates

Unity world space is left-handed, Y up, in metres. `Matrix4x4` is stored column-major; this is checked at runtime. The projection is `projectionMatrix * worldToCameraMatrix` in OpenGL convention, then mapped to the viewport and to IMGUI pixels with the origin top-left. Points behind the near plane and non-finite values are rejected. Every 30 frames the menu compares this projection with Unity's own `Camera.WorldToViewportPoint` and shows the difference in pixels.

## Verification status

- **Cross-compiled:** built with MinGW-w64 GCC 13 in a Linux container: 0 warnings, exit code 0. The DLL imports only `KERNEL32`, `msvcrt` and `USER32`. **The MSVC build has not been run.**
- **Isolated checks:** projection, box, UTF-16 and enum checks pass (25 cases).
- **Not run in the game.** First checks:
  - The INFO section lists no `!` problems apart from the version line.
  - "Projection error vs Unity" is about 0 px.
  - Boxes sit on players, including when they crouch.
