# SLInternal: SCP: Secret Laboratory client-side internal

Runs entirely inside your own game client; nothing on the server side. Features:
player ESP, item ESP, legit aim assist, auto shoot, with an in-game menu.

## How it works

| Piece | What it is |
|---|---|
| `native/injector` → `SLInjector.exe` | LoadLibraryW + CreateRemoteThread injector. Finds `SCPSL.exe` (the name is an assumption, pass another one as the first argument), refuses 32-bit targets, and confirms success by finding the DLL in the target's module list. |
| `native/bootstrap` → `SLBootstrap.dll` | Waits for Unity's Mono runtime (`mono-2.0-bdwgc.dll`; stops with a log line if it finds `GameAssembly.dll`, i.e. IL2CPP), loads `SLInternal.dll` from memory and calls `SLInternal.Loader.Init()`. It installs no hooks and stays loaded doing nothing afterwards. |
| `payload` → `SLInternal.dll` | Managed C# payload. Creates a hidden `GameObject` on Unity's main thread and does everything from `Update`/`LateUpdate`/`OnGUI`, using the game's own types. |

There are no hardcoded offsets. The payload compiles against **your** client's
`Assembly-CSharp.dll`, so if a member was renamed in your build, the compiler stops and
names it. All game access lives in `payload/src/GameApi.cs`.

## Build (Windows, x64)

Requirements: Visual Studio 2022 (Desktop C++ workload), CMake 3.20+, .NET SDK 6 or newer.

```bash
cmake -S native -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release
```

```bash
dotnet build payload/SLInternal.csproj -c Release -p:ManagedDir="C:\Program Files (x86)\Steam\steamapps\common\SCP Secret Laboratory\SCPSL_Data\Managed"
```

Use your actual install path for `ManagedDir`. All three outputs (`SLInjector.exe`,
`SLBootstrap.dll`, `SLInternal.dll`) end up in `dist/`. Keep them together: the bootstrap
loads `SLInternal.dll` from its own folder and writes its logs there.

## Use

1. Start the game and get to the main menu or into a server.
2. Run `dist\SLInjector.exe` (or `SLInjector.exe <process.exe> <path\to\SLBootstrap.dll>`).
3. Press **Insert** for the menu and **End** to unload.
4. Before using aim assist: spawn as a human, look roughly at the horizon, open
   **Settings → Calibrate** and keep your hands off the mouse for about 2 s. This measures how many
   degrees one mouse count turns your view. Then press **Save config**. Aim assist stays disabled
   until calibration succeeds, and its FOV circle is red until then.

Logs: `dist\SLBootstrap.log` (native side), `dist\SLInternal.log` and the Unity `Player.log`
(managed side, prefixed `[SLInternal]`). Settings: `dist\SLInternal.cfg`.

## Features

- **ESP**: box (from the player's `CharacterController` bounds; falls back to an approximate column built from
  body position and eye height), name, role, distance, health bar, snaplines. Colours follow the team.
  Teammates can be dimmed or hidden.
- **Item ESP**: every `ItemPickupBase`, with its `ItemType` name and distance, filtered by `ItemCategory`
  (keycards, firearms, medical, SCP items, grenades, armor, ammo, other).
- **Aim assist (legit)**: active only while its key is held (default Mouse2). Picks the target closest to the
  crosshair inside a cone (FOV is the half-angle in degrees), optionally only visible targets, and turns the view
  with exponential smoothing (`Speed`, per second) and a hard turn-rate cap. Aim point slides from body centre (0)
  to eye (1). Output is relative mouse input, so the game's own mouse look turns the camera.
- **Auto shoot**: casts the game's own hit-registration ray from your camera. If an enemy hitbox is the first thing
  it hits, it clicks the left mouse button after a reaction delay. Modes: tap (click length plus pause) or hold while
  on target. It never injects while you are already holding fire. Default: only while Mouse2 is held.

## What is verified and what is not

**Game members used directly**, so the compiler checks them on your build: `ReferenceHub.AllHubs`, `isLocalPlayer`,
`roleManager.CurrentRole` (`RoleTypeId`, `Team`), `IFpcRole.FpcModule.Position`, `PlayerCameraReference`,
`nicknameSync.DisplayName`, `playerStats.GetModule<HealthStat>()` (`CurValue`/`MaxValue`), `Team.GetFaction()`,
`ItemPickupBase.Info.ItemId`, `ItemPickupBase.OnPickupAdded`/`OnPickupDestroyed`,
`InventoryItemLoader.AvailableItems[...].Category`. Each of these is used by Northwood's open-source LabAPI
(commit `7b837be`, 2026-05-30), which compiles against the normal, non-publicized assembly.

**Game members looked up by reflection at runtime** (seen only in EXILED, which compiles against a publicized
assembly, so their access level is unknown): `HitboxIdentity`, `HitboxIdentity.TargetHub`,
`HitboxIdentity.IsEnemy(RoleTypeId, RoleTypeId)`, `HitscanHitregModuleBase.HitregMask`. **Settings → Status** shows
whether each one resolved. Fallbacks: `GetComponentInParent<ReferenceHub>`, faction comparison and
`Physics.DefaultRaycastLayers`.

**Checked in a Linux container, not in the game:**
- Native: built with MinGW-w64 GCC 13 using `-Wall -Wextra`, with no warnings. MSVC was not available there,
  so the MSVC build above has not been run.
- Payload: compiled with zero warnings against the real Unity 2021.3 reference assemblies (NuGet
  `UnityEngine.Modules 2021.3.33`) plus hand-written stand-ins for the game types that copy the shapes above.
  Your build will compile against the real `Assembly-CSharp`.
- Aim math (angles, wrap-around, smoothing, FOV radius, mouse remainder) passed isolated tests, and so did a
  simulated aim loop where the mouse input lands one frame late (30–240 FPS, no overshoot beyond one mouse count).

**Not verified in game:** everything that depends on the running client.

## In-game verification checklist (in order)

1. `SLBootstrap.log` ends with `Loader.Init returned`, and `SLInternal.log` has `Main component created` plus four
   Status lines.
2. **Settings → Status**: camera is `Camera.main` (if it says "nearest to eye", check that boxes line up), the local
   player shows your role, and the player count matches the server.
3. ESP boxes sit on players while you move. If they are offset, the projection camera is wrong; check the camera
   name in Status.
4. Calibration reports two non-zero values, and the Y value has the opposite sign of X for a normal (non-inverted)
   mouse.
5. Aim assist on a teammate (enable "Include teammates") to tune without shooting anyone.
6. Auto shoot: the "Crosshair: ON TARGET" label in the Auto shoot tab must switch only when your crosshair is on a
   player's body, not on walls next to them.

## Known limitations

- Health is shown only when the client actually receives it (`MaxValue > 0`); the server may not sync other
  players' health.
- I have not checked whether servers send accurate positions for players you can't see. If they don't, ESP for those
  players will be stale or missing.
- Mouse clicks inside the menu also reach the game, so open it when you're not holding a weapon you mind firing.
- Mono keeps an already-loaded assembly with the same name, so after rebuilding `SLInternal.dll`, restart the game.
  Unload (End) followed by re-injecting the same build works.
- Calibration assumes a linear mouse response. If the game applies its own mouse acceleration or smoothing,
  recalibrate and lower `Speed`.
