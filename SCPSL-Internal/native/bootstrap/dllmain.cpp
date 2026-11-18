// SLBootstrap.dll: loaded into the game process, waits for Unity's Mono runtime,
// loads SLInternal.dll (managed payload, same folder) from memory and calls
// SLInternal.Loader.Init() once. It installs no hooks and keeps nothing running.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "mono_api.h"

namespace
{
constexpr wchar_t kPayloadFile[] = L"SLInternal.dll";
constexpr wchar_t kLogFile[] = L"SLBootstrap.log";
constexpr char kLoaderNamespace[] = "SLInternal";
constexpr char kLoaderClass[] = "Loader";
constexpr char kLoaderMethod[] = "Init";
constexpr DWORD kMonoWaitMs = 120'000;
constexpr DWORD kDomainWaitMs = 60'000;
constexpr std::uint64_t kMaxPayloadBytes = 64ull * 1024 * 1024;

std::wstring g_dir;

void Log(const char* fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    SYSTEMTIME t{};
    GetLocalTime(&t);
    char line[1200];
    const int n = std::snprintf(line, sizeof(line), "[%02u:%02u:%02u.%03u] %s\r\n",
                                t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, msg);
    OutputDebugStringA(line);

    if (g_dir.empty() || n <= 0)
        return;
    const std::wstring path = g_dir + L"\\" + kLogFile;
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(f, line, static_cast<DWORD>(std::min<int>(n, static_cast<int>(sizeof(line) - 1))), &written, nullptr);
    CloseHandle(f);
}

std::wstring ModuleDirectory(HMODULE module)
{
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetModuleFileNameW(module, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0 || n >= buf.size())
        return {};
    std::wstring path(buf.data(), n);
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring{} : path.substr(0, slash);
}

std::string ToUtf8(const std::wstring& s)
{
    if (s.empty())
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

bool ReadWholeFile(const std::wstring& path, std::vector<char>& out)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart > 0 &&
              static_cast<std::uint64_t>(size.QuadPart) <= kMaxPayloadBytes;
    if (ok)
    {
        out.resize(static_cast<size_t>(size.QuadPart));
        size_t done = 0;
        while (ok && done < out.size())
        {
            DWORD got = 0;
            const DWORD want = static_cast<DWORD>(std::min<size_t>(out.size() - done, 1u << 20));
            ok = ReadFile(f, out.data() + done, want, &got, nullptr) && got > 0;
            done += got;
        }
        ok = ok && done == out.size();
    }
    CloseHandle(f);
    return ok;
}

template <typename T>
bool Resolve(HMODULE mono, const char* name, T& fn, bool required)
{
    fn = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(mono, name)));
    if (!fn && required)
        Log("missing Mono export: %s", name);
    return fn != nullptr || !required;
}

bool ResolveApi(HMODULE mono, MonoApi& api)
{
    bool ok = true;
    ok &= Resolve(mono, "mono_get_root_domain", api.get_root_domain, true);
    ok &= Resolve(mono, "mono_thread_attach", api.thread_attach, true);
    ok &= Resolve(mono, "mono_thread_detach", api.thread_detach, true);
    ok &= Resolve(mono, "mono_image_open_from_data", api.image_open_from_data, true);
    ok &= Resolve(mono, "mono_assembly_load_from_full", api.assembly_load_from_full, true);
    ok &= Resolve(mono, "mono_assembly_get_image", api.assembly_get_image, true);
    ok &= Resolve(mono, "mono_class_from_name", api.class_from_name, true);
    ok &= Resolve(mono, "mono_class_get_method_from_name", api.class_get_method_from_name, true);
    ok &= Resolve(mono, "mono_runtime_invoke", api.runtime_invoke, true);
    Resolve(mono, "mono_object_to_string", api.object_to_string, false);
    Resolve(mono, "mono_string_to_utf8", api.string_to_utf8, false);
    Resolve(mono, "mono_free", api.free, false);
    return ok;
}

HMODULE WaitForMono()
{
    static constexpr const wchar_t* kNames[] = {L"mono-2.0-bdwgc.dll", L"mono-2.0-sgen.dll", L"mono.dll"};
    const ULONGLONG deadline = GetTickCount64() + kMonoWaitMs;
    while (GetTickCount64() < deadline)
    {
        if (GetModuleHandleW(L"GameAssembly.dll"))
        {
            Log("GameAssembly.dll is loaded: this is an IL2CPP build, the Mono payload cannot run here");
            return nullptr;
        }
        for (const wchar_t* name : kNames)
        {
            if (HMODULE m = GetModuleHandleW(name))
            {
                Log("Mono runtime module: %s", ToUtf8(name).c_str());
                return m;
            }
        }
        Sleep(250);
    }
    Log("timed out waiting for a Mono runtime module");
    return nullptr;
}

void LogManagedException(const MonoApi& api, MonoObject* exc)
{
    if (!api.object_to_string || !api.string_to_utf8)
    {
        Log("Loader.Init threw a managed exception (text unavailable: mono_object_to_string missing)");
        return;
    }
    MonoObject* inner = nullptr;
    MonoString* text = api.object_to_string(exc, &inner);
    if (!text || inner)
    {
        Log("Loader.Init threw a managed exception (ToString failed)");
        return;
    }
    char* utf8 = api.string_to_utf8(text);
    Log("Loader.Init threw: %s", utf8 ? utf8 : "(null)");
    if (utf8 && api.free)
        api.free(utf8);
}

DWORD WINAPI Worker(LPVOID)
{
    Log("SLBootstrap started, pid %lu", GetCurrentProcessId());

    HMODULE mono = WaitForMono();
    if (!mono)
        return 1;

    MonoApi api;
    if (!ResolveApi(mono, api))
        return 2;

    MonoDomain* domain = nullptr;
    const ULONGLONG deadline = GetTickCount64() + kDomainWaitMs;
    while (!(domain = api.get_root_domain()) && GetTickCount64() < deadline)
        Sleep(250);
    if (!domain)
    {
        Log("Mono root domain never became available");
        return 3;
    }

    const std::wstring payloadPath = g_dir + L"\\" + kPayloadFile;
    std::vector<char> bytes;
    if (!ReadWholeFile(payloadPath, bytes))
    {
        Log("cannot read payload %s (error %lu)", ToUtf8(payloadPath).c_str(), GetLastError());
        return 4;
    }

    MonoThread* thread = api.thread_attach(domain);
    if (!thread)
    {
        Log("mono_thread_attach failed");
        return 5;
    }

    DWORD result = 0;
    MonoImageOpenStatus status = MONO_IMAGE_OK;
    MonoImage* rawImage = api.image_open_from_data(bytes.data(), static_cast<std::uint32_t>(bytes.size()), 1, &status);
    MonoAssembly* assembly = nullptr;
    if (!rawImage || status != MONO_IMAGE_OK)
    {
        Log("mono_image_open_from_data failed, status %d", static_cast<int>(status));
        result = 6;
    }
    else
    {
        const std::string pathUtf8 = ToUtf8(payloadPath);
        assembly = api.assembly_load_from_full(rawImage, pathUtf8.c_str(), &status, 0);
        if (!assembly || status != MONO_IMAGE_OK)
        {
            Log("mono_assembly_load_from_full failed, status %d", static_cast<int>(status));
            result = 7;
        }
    }

    if (result == 0)
    {
        // If an assembly with the same name is already loaded, Mono returns that one,
        // so a rebuilt payload only takes effect after a game restart.
        MonoImage* image = api.assembly_get_image(assembly);
        MonoClass* klass = image ? api.class_from_name(image, kLoaderNamespace, kLoaderClass) : nullptr;
        MonoMethod* init = klass ? api.class_get_method_from_name(klass, kLoaderMethod, 0) : nullptr;
        if (!init)
        {
            Log("could not find %s.%s.%s()", kLoaderNamespace, kLoaderClass, kLoaderMethod);
            result = 8;
        }
        else
        {
            SetEnvironmentVariableW(L"SLINTERNAL_DIR", g_dir.c_str());
            MonoObject* exc = nullptr;
            api.runtime_invoke(init, nullptr, nullptr, &exc);
            if (exc)
            {
                LogManagedException(api, exc);
                result = 9;
            }
            else
            {
                Log("Loader.Init returned; see SLInternal.log for the managed side");
            }
        }
    }

    api.thread_detach(thread);
    return result;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        g_dir = ModuleDirectory(module);
        if (HANDLE t = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr))
            CloseHandle(t);
    }
    return TRUE;
}
