// SLMenu: internal IL2CPP menu for the SCP: Secret Laboratory client.
// Hooks StaticUnityMethods.OnGUI (game code, resolved by name) and draws through Unity IMGUI.

#include "esp.h"
#include "game.h"
#include "gui.h"
#include "il2cpp_api.h"
#include "log.h"
#include "menu.h"
#include "runtime.h"

#include <MinHook.h>
#include <windows.h>

#include <atomic>
#include <string>

namespace slm {

namespace {

using OnGuiFn = void (*)(Il2CppObject* self, const MethodInfo* method);

HMODULE g_self = nullptr;
void* g_target = nullptr;
OnGuiFn g_original = nullptr;
std::atomic<bool> g_active{false};
std::atomic<bool> g_unloading{false};
bool g_versionRead = false;
int g_failingFrames = 0;
std::string g_lastFrameError;

constexpr DWORD kWaitModuleMs = 120000;
constexpr DWORD kWaitImageMs = 180000;
constexpr int kDisableAfterFailingFrames = 600;

void ReadVersionOnce()
{
    if (g_versionRead)
        return;
    g_versionRead = true;
    if (!game::refs.versionString)
        return;
    Il2CppString* s = nullptr;
    il2cpp::api.field_static_get_value(game::refs.versionString, &s);
    const std::string v = rt::ToUtf8(s);
    SLM_LOG("GameCore.Version.VersionString = %s", v.empty() ? "(empty)" : v.c_str());
    game::refs.problems.insert(game::refs.problems.begin(), "game version " + (v.empty() ? std::string("?") : v));
}

bool IsRepaint()
{
    const game::Refs& r = game::refs;
    if (!r.eventGetCurrent || !r.eventGetType || r.eventTypeRepaint < 0)
        return true; // cannot tell: draw on every OnGUI call (non-repaint draws are discarded by Unity)
    Il2CppObject* ev = rt::Invoke(r.eventGetCurrent, nullptr);
    if (!ev)
        return false;
    Il2CppObject* type = rt::Invoke(r.eventGetType, ev);
    return type && r.eventTypeKind.Read(il2cpp::api.object_unbox(type)) == r.eventTypeRepaint;
}

void Frame()
{
    try {
        menu::HandleInput();
        if (menu::UnloadRequested())
            return;
        if (!IsRepaint())
            return;
        ReadVersionOnce();
        if (!gui::BeginFrame())
            return;
        try {
            esp::View view;
            if (game::refs.camera && esp::BuildView(view)) {
                esp::DrawPlayers(view);
                esp::DrawItems(view);
            }
            menu::Draw();
        } catch (...) {
            try { gui::EndFrame(); } catch (...) {}
            throw;
        }
        gui::EndFrame();
        g_failingFrames = 0;
    } catch (const std::exception& e) {
        if (g_lastFrameError != e.what()) {
            g_lastFrameError = e.what();
            SLM_LOG("frame error: %s", e.what());
        }
        if (++g_failingFrames > kDisableAfterFailingFrames) {
            g_active = false;
            SLM_LOG("drawing stopped after %d failing frames", g_failingFrames);
        }
    }
}

#if defined(_MSC_VER)
void GuardedFrame()
{
    __try {
        Frame();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_active = false;
        SLM_LOG("native exception 0x%08lX in frame; drawing stopped", GetExceptionCode());
    }
}
#else
void GuardedFrame()
{
    Frame();
}
#endif

DWORD WINAPI UnloadThread(LPVOID)
{
    // Disabling restores the original OnGUI bytes. The DLL, its trampoline and its code stay mapped:
    // a main-thread call that entered the detour just before the patch could still be returning through them.
    const MH_STATUS st = MH_DisableHook(g_target);
    SLM_LOG("unload: MH_DisableHook = %s; DLL stays loaded", MH_StatusToString(st));
    return 0;
}

void OnGuiDetour(Il2CppObject* self, const MethodInfo* method)
{
    g_original(self, method);
    if (!g_active.load(std::memory_order_relaxed))
        return;
    GuardedFrame();
    if (menu::UnloadRequested() && !g_unloading.exchange(true)) {
        g_active = false;
        gui::Shutdown();
        if (HANDLE t = CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr))
            CloseHandle(t);
    }
}

template <class F>
bool WaitFor(F condition, DWORD timeoutMs)
{
    const ULONGLONG end = GetTickCount64() + timeoutMs;
    while (!condition()) {
        if (GetTickCount64() > end)
            return false;
        Sleep(250);
    }
    return true;
}

bool InstallHook()
{
    const game::Refs& r = game::refs;
    void* onGui = rt::MethodPointer(r.sumOnGui);
    if (!onGui) {
        SLM_LOG("StaticUnityMethods.OnGUI has no code pointer inside GameAssembly.dll");
        return false;
    }
    // Identical-code folding would make several methods share one body; hooking it would also run on
    // Update/LateUpdate/FixedUpdate. Refuse in that case.
    for (const MethodInfo* other : {r.sumUpdate, r.sumLateUpdate, r.sumFixedUpdate}) {
        if (other && rt::MethodPointer(other) == onGui) {
            SLM_LOG("StaticUnityMethods.OnGUI shares its code with another method; not hooking");
            return false;
        }
    }

    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        SLM_LOG("MH_Initialize: %s", MH_StatusToString(st));
        return false;
    }
    st = MH_CreateHook(onGui, reinterpret_cast<void*>(&OnGuiDetour), reinterpret_cast<void**>(&g_original));
    if (st != MH_OK) {
        SLM_LOG("MH_CreateHook: %s", MH_StatusToString(st));
        return false;
    }
    g_target = onGui;
    g_active = true;
    st = MH_EnableHook(onGui);
    if (st != MH_OK) {
        g_active = false;
        MH_RemoveHook(onGui);
        g_target = nullptr;
        SLM_LOG("MH_EnableHook: %s", MH_StatusToString(st));
        return false;
    }
    SLM_LOG("hooked StaticUnityMethods.OnGUI at GameAssembly+0x%llX",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(onGui) -
                                            reinterpret_cast<uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"))));
    return true;
}

DWORD WINAPI InitThread(LPVOID)
{
    log::Open(g_self);
    SLM_LOG("SLMenu starting (pid %lu)", GetCurrentProcessId());

    HMODULE ga = nullptr;
    if (!WaitFor([&] { return (ga = GetModuleHandleW(L"GameAssembly.dll")) != nullptr; }, kWaitModuleMs)) {
        SLM_LOG("GameAssembly.dll not loaded: not an IL2CPP process or wrong target");
        return 0;
    }

    std::string missing;
    if (!il2cpp::ResolveExports(ga, missing)) {
        SLM_LOG("missing il2cpp exports: %s", missing.c_str());
        return 0;
    }
    rt::SetGameAssemblyRange(ga);

    if (!WaitFor([] { return il2cpp::api.domain_get() != nullptr; }, kWaitImageMs)) {
        SLM_LOG("il2cpp domain never became available");
        return 0;
    }
    Il2CppThread* thread = il2cpp::api.thread_attach(il2cpp::api.domain_get());
    if (!WaitFor([] { return rt::FindImage("Assembly-CSharp.dll") != nullptr; }, kWaitImageMs)) {
        SLM_LOG("Assembly-CSharp.dll image never appeared");
        il2cpp::api.thread_detach(thread);
        return 0;
    }

    const bool resolved = game::Resolve();
    if (!resolved)
        SLM_LOG("resolution failed; menu cannot draw. Problems are listed above.");
    else if (!InstallHook())
        SLM_LOG("hook not installed; SLMenu inactive");
    il2cpp::api.thread_detach(thread);
    SLM_LOG("init done");
    return 0;
}

} // namespace

} // namespace slm

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        slm::g_self = module;
        DisableThreadLibraryCalls(module);
        // Pin the module: hooks and the trampoline must outlive any FreeLibrary by the injector.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        if (HANDLE t = CreateThread(nullptr, 0, slm::InitThread, nullptr, 0, nullptr))
            CloseHandle(t);
    }
    return TRUE;
}
