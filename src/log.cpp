#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace slm::log {

static std::mutex g_mutex;
static FILE* g_file = nullptr;

void Open(void* moduleHandle)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file)
        return;

    wchar_t path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(static_cast<HMODULE>(moduleHandle), path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
        return;
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"SLMenu.log");
    g_file = _wfopen(path, L"w");
}

void Write(const char* fmt, ...)
{
    char line[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    SYSTEMTIME t;
    GetLocalTime(&t);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file)
        return;
    std::fprintf(g_file, "[%02u:%02u:%02u.%03u] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
    std::fflush(g_file);
}

void Close()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

} // namespace slm::log
