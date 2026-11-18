#pragma once

#include <cstdint>

// Opaque Mono embedding types. These are the public Mono C API (mono/metadata/*.h),
// exported by Unity's mono-2.0-bdwgc.dll; nothing here is game-specific.
struct MonoDomain;
struct MonoAssembly;
struct MonoImage;
struct MonoClass;
struct MonoMethod;
struct MonoObject;
struct MonoThread;
struct MonoString;

using mono_bool = std::int32_t;

enum MonoImageOpenStatus : std::int32_t
{
    MONO_IMAGE_OK = 0,
    MONO_IMAGE_ERROR_ERRNO = 1,
    MONO_IMAGE_MISSING_ASSEMBLYREF = 2,
    MONO_IMAGE_IMAGE_INVALID = 3,
};

struct MonoApi
{
    MonoDomain* (*get_root_domain)() = nullptr;
    MonoThread* (*thread_attach)(MonoDomain*) = nullptr;
    void (*thread_detach)(MonoThread*) = nullptr;
    MonoImage* (*image_open_from_data)(char* data, std::uint32_t len, mono_bool need_copy, MonoImageOpenStatus* status) = nullptr;
    MonoAssembly* (*assembly_load_from_full)(MonoImage* image, const char* fname, MonoImageOpenStatus* status, mono_bool refonly) = nullptr;
    MonoImage* (*assembly_get_image)(MonoAssembly*) = nullptr;
    MonoClass* (*class_from_name)(MonoImage* image, const char* name_space, const char* name) = nullptr;
    MonoMethod* (*class_get_method_from_name)(MonoClass* klass, const char* name, int param_count) = nullptr;
    MonoObject* (*runtime_invoke)(MonoMethod* method, void* obj, void** params, MonoObject** exc) = nullptr;

    // Optional: only used to print a managed exception's text.
    MonoString* (*object_to_string)(MonoObject* obj, MonoObject** exc) = nullptr;
    char* (*string_to_utf8)(MonoString* str) = nullptr;
    void (*free)(void* ptr) = nullptr;
};
