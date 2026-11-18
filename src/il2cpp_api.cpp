#include "il2cpp_api.h"

#include <windows.h>

namespace slm::il2cpp {

Api api;

bool ResolveExports(void* gameAssembly, std::string& missing)
{
    auto module = static_cast<HMODULE>(gameAssembly);
    bool ok = true;
#define SLM_RESOLVE(ret, name, params, required)                                               \
    api.name = reinterpret_cast<ret(*) params>(GetProcAddress(module, "il2cpp_" #name));        \
    if (!api.name && required) {                                                                \
        ok = false;                                                                             \
        missing += "il2cpp_" #name " ";                                                         \
    }
    SLM_IL2CPP_API(SLM_RESOLVE)
#undef SLM_RESOLVE
    return ok;
}

} // namespace slm::il2cpp
