#pragma once
// The il2cpp_* C API exported by GameAssembly.dll (libil2cpp public API, il2cpp-api-functions.h).
// Only opaque handles are used; no il2cpp struct layouts are assumed except where validated at runtime
// (see runtime.cpp: object header size, array header, MethodInfo::methodPointer).

#include <cstddef>
#include <cstdint>
#include <string>

struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppImage;
struct Il2CppClass;
struct Il2CppType;
struct MethodInfo;
struct FieldInfo;
struct Il2CppObject;
struct Il2CppString;
struct Il2CppArray;
struct Il2CppThread;
struct Il2CppException;

namespace slm::il2cpp {

// X(return type, name without il2cpp_ prefix, parameter list, required)
#define SLM_IL2CPP_API(X)                                                                                   \
    X(Il2CppDomain*, domain_get, (), true)                                                                  \
    X(const Il2CppAssembly**, domain_get_assemblies, (const Il2CppDomain*, size_t*), true)                 \
    X(const Il2CppImage*, assembly_get_image, (const Il2CppAssembly*), true)                                \
    X(const char*, image_get_name, (const Il2CppImage*), true)                                              \
    X(Il2CppClass*, class_from_name, (const Il2CppImage*, const char*, const char*), true)                  \
    X(FieldInfo*, class_get_field_from_name, (Il2CppClass*, const char*), true)                             \
    X(FieldInfo*, class_get_fields, (Il2CppClass*, void**), true)                                           \
    X(const MethodInfo*, class_get_methods, (Il2CppClass*, void**), true)                                   \
    X(Il2CppClass*, class_get_nested_types, (Il2CppClass*, void**), true)                                   \
    X(Il2CppClass*, class_get_parent, (Il2CppClass*), true)                                                 \
    X(bool, class_is_assignable_from, (Il2CppClass*, Il2CppClass*), true)                                   \
    X(bool, class_is_valuetype, (const Il2CppClass*), true)                                                 \
    X(int32_t, class_value_size, (Il2CppClass*, uint32_t*), true)                                           \
    X(const Il2CppType*, class_get_type, (Il2CppClass*), true)                                              \
    X(const char*, class_get_name, (Il2CppClass*), true)                                                    \
    X(const char*, class_get_namespace, (Il2CppClass*), true)                                               \
    X(const char*, method_get_name, (const MethodInfo*), true)                                              \
    X(uint32_t, method_get_param_count, (const MethodInfo*), true)                                          \
    X(const Il2CppType*, method_get_param, (const MethodInfo*, uint32_t), true)                             \
    X(char*, type_get_name, (const Il2CppType*), true)                                                      \
    X(Il2CppObject*, type_get_object, (const Il2CppType*), true)                                            \
    X(void, free, (void*), true)                                                                            \
    X(const char*, field_get_name, (FieldInfo*), true)                                                      \
    X(int, field_get_flags, (FieldInfo*), true)                                                             \
    X(size_t, field_get_offset, (FieldInfo*), true)                                                         \
    X(const Il2CppType*, field_get_type, (FieldInfo*), true)                                                \
    X(void, field_get_value, (Il2CppObject*, FieldInfo*, void*), true)                                      \
    X(void, field_static_get_value, (FieldInfo*, void*), true)                                              \
    X(Il2CppObject*, runtime_invoke, (const MethodInfo*, void*, void**, Il2CppException**), true)           \
    X(const MethodInfo*, object_get_virtual_method, (Il2CppObject*, const MethodInfo*), true)               \
    X(Il2CppClass*, object_get_class, (Il2CppObject*), true)                                                \
    X(void*, object_unbox, (Il2CppObject*), true)                                                           \
    X(Il2CppObject*, object_new, (const Il2CppClass*), true)                                                \
    X(Il2CppObject*, value_box, (Il2CppClass*, void*), true)                                                \
    X(Il2CppString*, string_new, (const char*), true)                                                       \
    X(int32_t, string_length, (Il2CppString*), true)                                                        \
    X(uint16_t*, string_chars, (Il2CppString*), true)                                                       \
    X(uint32_t, array_length, (Il2CppArray*), true)                                                         \
    X(Il2CppThread*, thread_attach, (Il2CppDomain*), true)                                                  \
    X(void, thread_detach, (Il2CppThread*), true)                                                           \
    X(void, format_exception, (const Il2CppException*, char*, int), false)                                  \
    /* GC handles are uint32_t in older libil2cpp and pointer-sized in newer; uintptr_t covers both on x64 */ \
    X(uintptr_t, gchandle_new, (Il2CppObject*, bool), false)                                                \
    X(Il2CppObject*, gchandle_get_target, (uintptr_t), false)                                               \
    X(void, gchandle_free, (uintptr_t), false)

struct Api {
#define SLM_DECLARE(ret, name, params, required) ret(*name) params = nullptr;
    SLM_IL2CPP_API(SLM_DECLARE)
#undef SLM_DECLARE
};

extern Api api;

// Resolves every export from GameAssembly.dll. Returns false and lists missing required exports otherwise.
bool ResolveExports(void* gameAssembly, std::string& missing);

} // namespace slm::il2cpp
