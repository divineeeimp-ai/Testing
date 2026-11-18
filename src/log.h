#pragma once
#include <string>

namespace slm::log {

// Opens SLMenu.log next to the DLL. Safe to call more than once.
void Open(void* moduleHandle);
void Write(const char* fmt, ...);
void Close();

} // namespace slm::log

#define SLM_LOG(...) ::slm::log::Write(__VA_ARGS__)
