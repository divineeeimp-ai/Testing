// SLInject: loads SLMenu.dll into a running SCPSL.exe with LoadLibraryW in a remote thread.
// Usage: SLInject.exe [path\to\SLMenu.dll] [process name, default SCPSL.exe]

#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace {

DWORD FindProcess(const wchar_t* exeName, int& matches)
{
    matches = 0;
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, exeName) == 0) {
            pid = pe.th32ProcessID;
            ++matches;
        }
    }
    CloseHandle(snap);
    return pid;
}

bool HasModule(DWORD pid, const wchar_t* moduleName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &me); ok && !found; ok = Module32NextW(snap, &me))
        found = _wcsicmp(me.szModule, moduleName) == 0;
    CloseHandle(snap);
    return found;
}

int Fail(const char* what)
{
    std::fprintf(stderr, "error: %s (GetLastError=%lu)\n", what, GetLastError());
    return 1;
}

std::wstring Describe(DWORD err)
{
    wchar_t* text = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, err, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                   reinterpret_cast<LPWSTR>(&text), 0, nullptr);
    std::wstring s = n && text ? text : L"(no system text)";
    if (text)
        LocalFree(text);
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r'))
        s.pop_back();
    return s;
}

void Explain(DWORD err)
{
    switch (err) {
    case ERROR_INVALID_IMAGE_HASH: // 577
        std::fprintf(stderr, "  -> the game only accepts signed DLLs (code-integrity / signature policy).\n");
        break;
    case ERROR_VIRUS_INFECTED:          // 225
    case ERROR_VIRUS_DELETED:           // 226
        std::fprintf(stderr, "  -> antivirus blocked the DLL. Check Windows Security > Protection history.\n");
        break;
    case ERROR_MOD_NOT_FOUND: // 126
        std::fprintf(stderr, "  -> the DLL or one of its dependencies was not found from inside the game.\n");
        break;
    case ERROR_ACCESS_DENIED: // 5
        std::fprintf(stderr, "  -> the game process may not read the DLL's folder. Copy the DLL to C:\\SLMenu and retry.\n");
        break;
    case ERROR_BAD_EXE_FORMAT: // 193
        std::fprintf(stderr, "  -> wrong architecture or damaged file; rebuild with -A x64.\n");
        break;
    case ERROR_DLL_INIT_FAILED: // 1114
        std::fprintf(stderr, "  -> DllMain returned FALSE or crashed.\n");
        break;
    default:
        break;
    }
}

void PrintMitigations(HANDLE process)
{
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY sig{};
    if (GetProcessMitigationPolicy(process, ProcessSignaturePolicy, &sig, sizeof(sig)))
        std::printf("target policy: MicrosoftSignedOnly=%lu StoreSignedOnly=%lu\n",
                    static_cast<unsigned long>(sig.MicrosoftSignedOnly), static_cast<unsigned long>(sig.StoreSignedOnly));
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY img{};
    if (GetProcessMitigationPolicy(process, ProcessImageLoadPolicy, &img, sizeof(img)))
        std::printf("target policy: NoRemoteImages=%lu NoLowMandatoryLabelImages=%lu\n",
                    static_cast<unsigned long>(img.NoRemoteImages), static_cast<unsigned long>(img.NoLowMandatoryLabelImages));
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dyn{};
    if (GetProcessMitigationPolicy(process, ProcessDynamicCodePolicy, &dyn, sizeof(dyn)))
        std::printf("target policy: ProhibitDynamicCode=%lu\n", static_cast<unsigned long>(dyn.ProhibitDynamicCode));
}

void Put64(std::vector<uint8_t>& v, uint64_t x)
{
    for (int i = 0; i < 8; ++i)
        v.push_back(static_cast<uint8_t>(x >> (8 * i)));
}

// x64 thread routine; RCX = path (thread parameter).
//   sub rsp,28h ; mov rax,LoadLibraryW ; call rax ; mov rcx,&result ; mov [rcx],rax
//   mov rax,GetLastError ; call rax ; mov rcx,&result+8 ; mov [rcx],eax ; xor eax,eax ; add rsp,28h ; ret
std::vector<uint8_t> BuildStub(void* loadLibraryW, void* getLastError, uint8_t* result)
{
    std::vector<uint8_t> s;
    s.insert(s.end(), {0x48, 0x83, 0xEC, 0x28});
    s.insert(s.end(), {0x48, 0xB8}); Put64(s, reinterpret_cast<uint64_t>(loadLibraryW));
    s.insert(s.end(), {0xFF, 0xD0});
    s.insert(s.end(), {0x48, 0xB9}); Put64(s, reinterpret_cast<uint64_t>(result));
    s.insert(s.end(), {0x48, 0x89, 0x01});
    s.insert(s.end(), {0x48, 0xB8}); Put64(s, reinterpret_cast<uint64_t>(getLastError));
    s.insert(s.end(), {0xFF, 0xD0});
    s.insert(s.end(), {0x48, 0xB9}); Put64(s, reinterpret_cast<uint64_t>(result + 8));
    s.insert(s.end(), {0x89, 0x01});
    s.insert(s.end(), {0x31, 0xC0});
    s.insert(s.end(), {0x48, 0x83, 0xC4, 0x28});
    s.push_back(0xC3);
    return s;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    std::wstring dll = argc > 1 ? argv[1] : L"SLMenu.dll";
    const wchar_t* exe = argc > 2 ? argv[2] : L"SCPSL.exe";

    wchar_t full[MAX_PATH] = {};
    DWORD n = GetFullPathNameW(dll.c_str(), MAX_PATH, full, nullptr);
    if (n == 0 || n >= MAX_PATH)
        return Fail("bad DLL path");
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES)
        return Fail("DLL not found");

    int matches = 0;
    const DWORD pid = FindProcess(exe, matches);
    if (!pid) {
        std::fwprintf(stderr, L"error: %ls is not running\n", exe);
        return 1;
    }
    if (matches > 1) {
        std::fwprintf(stderr, L"error: %d processes named %ls; close the extra ones\n", matches, exe);
        return 1;
    }
    if (!HasModule(pid, L"GameAssembly.dll"))
        std::fwprintf(stderr, L"warning: GameAssembly.dll not loaded yet in pid %lu; SLMenu will wait for it\n", pid);

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                     PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process)
        return Fail("OpenProcess");

    BOOL wow64 = FALSE;
    if (IsWow64Process(process, &wow64) && wow64) {
        CloseHandle(process);
        std::fprintf(stderr, "error: target is 32-bit; SLMenu is x64\n");
        return 1;
    }

    PrintMitigations(process);

    // Data block in the target: [0] HMODULE result, [8] GetLastError, [16...] DLL path (UTF-16).
    const SIZE_T pathBytes = (wcslen(full) + 1) * sizeof(wchar_t);
    const SIZE_T dataBytes = 16 + pathBytes;
    auto* data = static_cast<uint8_t*>(VirtualAllocEx(process, nullptr, dataBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!data) {
        CloseHandle(process);
        return Fail("VirtualAllocEx (data)");
    }
    std::vector<uint8_t> block(dataBytes, 0);
    std::memcpy(block.data() + 16, full, pathBytes);
    SIZE_T written = 0;
    if (!WriteProcessMemory(process, data, block.data(), dataBytes, &written) || written != dataBytes) {
        VirtualFreeEx(process, data, 0, MEM_RELEASE);
        CloseHandle(process);
        return Fail("WriteProcessMemory (data)");
    }

    // kernel32 and ntdll are mapped at the same address in every process of a boot session.
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    void* loadLibraryW = reinterpret_cast<void*>(GetProcAddress(k32, "LoadLibraryW"));
    void* getLastError = reinterpret_cast<void*>(GetProcAddress(k32, "GetLastError"));

    // Preferred: a 64-byte stub that calls LoadLibraryW(path) and stores the HMODULE and GetLastError,
    // so a failure reports the real reason. Falls back to a plain LoadLibraryW thread if the target
    // refuses executable memory.
    const std::vector<uint8_t> stub = BuildStub(loadLibraryW, getLastError, data);
    void* code = VirtualAllocEx(process, nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    DWORD oldProtect = 0;
    bool useStub = code && WriteProcessMemory(process, code, stub.data(), stub.size(), &written) &&
                   written == stub.size() &&
                   VirtualProtectEx(process, code, stub.size(), PAGE_EXECUTE_READ, &oldProtect) &&
                   FlushInstructionCache(process, code, stub.size());
    if (!useStub)
        std::fprintf(stderr, "note: could not place the diagnostic stub (GetLastError=%lu); using plain LoadLibraryW\n",
                     GetLastError());

    auto start = reinterpret_cast<LPTHREAD_START_ROUTINE>(useStub ? code : loadLibraryW);
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, start, data + 16, 0, nullptr);
    if (!thread) {
        const DWORD err = GetLastError();
        if (code)
            VirtualFreeEx(process, code, 0, MEM_RELEASE);
        VirtualFreeEx(process, data, 0, MEM_RELEASE);
        CloseHandle(process);
        SetLastError(err);
        return Fail("CreateRemoteThread");
    }
    const DWORD wait = WaitForSingleObject(thread, 30000);
    CloseHandle(thread);

    uint64_t result[2] = {};
    SIZE_T read = 0;
    const bool haveResult = useStub && wait == WAIT_OBJECT_0 &&
                            ReadProcessMemory(process, data, result, sizeof(result), &read) && read == sizeof(result);
    if (wait == WAIT_OBJECT_0) {
        if (code)
            VirtualFreeEx(process, code, 0, MEM_RELEASE);
        VirtualFreeEx(process, data, 0, MEM_RELEASE);
    }
    CloseHandle(process);

    const wchar_t* fileName = wcsrchr(full, L'\\');
    fileName = fileName ? fileName + 1 : full;
    if (wait != WAIT_OBJECT_0) {
        std::fprintf(stderr, "error: remote thread did not finish within 30 s (the game may have suspended or blocked it)\n");
        return 1;
    }
    if (!HasModule(pid, fileName)) {
        if (haveResult) {
            const DWORD err = static_cast<DWORD>(result[1]);
            std::fwprintf(stderr, L"error: LoadLibraryW failed inside the game: error %lu: %ls\n", err, Describe(err).c_str());
            Explain(err);
        } else {
            std::fprintf(stderr, "error: LoadLibraryW failed inside the game (reason unavailable without the stub)\n");
        }
        return 1;
    }
    std::wprintf(L"injected %ls into %ls (pid %lu). Log: SLMenu.log next to the DLL.\n", full, exe, pid);
    return 0;
}
