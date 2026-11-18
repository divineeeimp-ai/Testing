#include "runtime.h"

#include "log.h"
#include "text.h"

#include <windows.h>
#include <psapi.h>

#include <cstring>

namespace slm::rt {

using il2cpp::api;

namespace {

FieldInfo* g_cachedPtrField = nullptr;
size_t g_valueFieldBase = static_cast<size_t>(-1);
uintptr_t g_gaBegin = 0;
uintptr_t g_gaEnd = 0;

constexpr int kFieldStatic = 0x10;  // FIELD_ATTRIBUTE_STATIC
constexpr int kFieldLiteral = 0x40; // FIELD_ATTRIBUTE_LITERAL

// il2cpp szarray header on x64: Il2CppObject (klass, monitor) = 16, bounds* = 8, max_length = 8.
constexpr size_t kArrayBoundsOffset = 16;
constexpr size_t kArrayLengthOffset = 24;
constexpr size_t kArrayDataOffset = 32;

bool ParamsMatch(const MethodInfo* m, std::initializer_list<const char*> params)
{
    if (api.method_get_param_count(m) != params.size())
        return false;
    uint32_t i = 0;
    for (const char* expected : params) {
        if (TypeName(api.method_get_param(m, i++)) != expected)
            return false;
    }
    return true;
}

void LogOverloads(Il2CppClass* klass, const char* name)
{
    for (Il2CppClass* k = klass; k; k = api.class_get_parent(k)) {
        void* iter = nullptr;
        while (const MethodInfo* m = api.class_get_methods(k, &iter)) {
            if (std::strcmp(api.method_get_name(m), name) != 0)
                continue;
            std::string sig;
            for (uint32_t i = 0; i < api.method_get_param_count(m); ++i)
                sig += (i ? ", " : "") + TypeName(api.method_get_param(m, i));
            SLM_LOG("    candidate %s.%s(%s)", api.class_get_name(k), name, sig.c_str());
        }
    }
}

bool FieldOffset(Il2CppClass* klass, const char* name, size_t& offset)
{
    FieldInfo* f = api.class_get_field_from_name(klass, name);
    if (!f)
        return false;
    offset = api.field_get_offset(f);
    return true;
}

} // namespace

const Il2CppImage* FindImage(const char* name)
{
    Il2CppDomain* domain = api.domain_get();
    if (!domain)
        return nullptr;
    size_t count = 0;
    const Il2CppAssembly** assemblies = api.domain_get_assemblies(domain, &count);
    if (!assemblies || count > 4096)
        return nullptr;
    for (size_t i = 0; i < count; ++i) {
        const Il2CppImage* image = api.assembly_get_image(assemblies[i]);
        const char* imageName = image ? api.image_get_name(image) : nullptr;
        if (imageName && std::strcmp(imageName, name) == 0)
            return image;
    }
    return nullptr;
}

Il2CppClass* FindClass(const Il2CppImage* image, const char* ns, const char* name)
{
    if (!image)
        return nullptr;
    Il2CppClass* k = api.class_from_name(image, ns, name);
    if (!k)
        SLM_LOG("  missing class %s%s%s", ns, *ns ? "." : "", name);
    return k;
}

std::string TypeName(const Il2CppType* type)
{
    if (!type)
        return "?";
    char* raw = api.type_get_name(type);
    if (!raw)
        return "?";
    std::string s(raw);
    api.free(raw);
    return s;
}

const MethodInfo* FindMethod(Il2CppClass* klass, const char* name, std::initializer_list<const char*> params)
{
    if (!klass)
        return nullptr;
    for (Il2CppClass* k = klass; k; k = api.class_get_parent(k)) {
        void* iter = nullptr;
        while (const MethodInfo* m = api.class_get_methods(k, &iter)) {
            if (std::strcmp(api.method_get_name(m), name) == 0 && ParamsMatch(m, params))
                return m;
        }
    }
    std::string sig;
    for (const char* p : params)
        sig += (sig.empty() ? "" : ", ") + std::string(p);
    SLM_LOG("  missing method %s.%s(%s)", api.class_get_name(klass), name, sig.c_str());
    LogOverloads(klass, name);
    return nullptr;
}

FieldInfo* FindField(Il2CppClass* klass, const char* name)
{
    for (Il2CppClass* k = klass; k; k = api.class_get_parent(k)) {
        if (FieldInfo* f = api.class_get_field_from_name(k, name))
            return f;
    }
    if (klass)
        SLM_LOG("  missing field %s.%s", api.class_get_name(klass), name);
    return nullptr;
}

Il2CppObject* TypeObject(Il2CppClass* klass)
{
    return klass ? api.type_get_object(api.class_get_type(klass)) : nullptr;
}

Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args, bool virtualCall)
{
    if (!m)
        throw Error("method not resolved");
    if (virtualCall) {
        if (!obj)
            throw Error("virtual call on null object");
        m = api.object_get_virtual_method(static_cast<Il2CppObject*>(obj), m);
        if (!m)
            throw Error("virtual method not found on runtime class");
    }

    Il2CppException* exc = nullptr;
    Il2CppObject* result = api.runtime_invoke(m, obj, args, &exc);
    if (exc) {
        char text[512] = "managed exception";
        if (api.format_exception)
            api.format_exception(exc, text, sizeof(text));
        throw Error(std::string(api.method_get_name(m)) + ": " + text);
    }
    return result;
}

void SetUnityObjectClass(Il2CppClass* unityObject)
{
    g_cachedPtrField = unityObject ? api.class_get_field_from_name(unityObject, "m_CachedPtr") : nullptr;
}

bool IsUnityAlive(Il2CppObject* obj)
{
    if (!obj || !g_cachedPtrField)
        return false;
    return GetField<void*>(obj, g_cachedPtrField) != nullptr;
}

bool ArrayElements(Il2CppObject* array, uint32_t maxCount, std::vector<Il2CppObject*>& out)
{
    out.clear();
    if (!array)
        return false;
    auto base = reinterpret_cast<const uint8_t*>(array);
    const uint32_t n = api.array_length(reinterpret_cast<Il2CppArray*>(array));
    uintptr_t headerLength = 0;
    void* bounds = nullptr;
    std::memcpy(&headerLength, base + kArrayLengthOffset, sizeof(headerLength));
    std::memcpy(&bounds, base + kArrayBoundsOffset, sizeof(bounds));
    if (headerLength != n || bounds != nullptr)
        throw Error("unexpected il2cpp array header layout");
    if (n > maxCount)
        return false;
    out.resize(n);
    if (n)
        std::memcpy(out.data(), base + kArrayDataOffset, n * sizeof(Il2CppObject*));
    return true;
}

std::string ToUtf8(Il2CppString* s)
{
    if (!s)
        return {};
    const int32_t n = api.string_length(s);
    if (n <= 0 || n > 4096)
        return {};
    return Utf16ToUtf8(api.string_chars(s), static_cast<size_t>(n));
}

Il2CppString* NewString(const std::string& utf8)
{
    return api.string_new(utf8.c_str());
}

bool EnumUnderlying(Il2CppClass* enumClass, int& size, bool& isSigned)
{
    size = 0;
    isSigned = false;
    FieldInfo* valueField = enumClass ? api.class_get_field_from_name(enumClass, "value__") : nullptr;
    if (!valueField)
        return false;
    const std::string underlying = TypeName(api.field_get_type(valueField));
    if (underlying == "System.Byte") size = 1;
    else if (underlying == "System.SByte") size = 1, isSigned = true;
    else if (underlying == "System.Int16") size = 2, isSigned = true;
    else if (underlying == "System.UInt16") size = 2;
    else if (underlying == "System.Int32") size = 4, isSigned = true;
    else if (underlying == "System.UInt32") size = 4;
    else if (underlying == "System.Int64") size = 8, isSigned = true;
    else if (underlying == "System.UInt64") size = 8;
    else return false;
    return true;
}

Il2CppClass* FindNested(Il2CppClass* outer, const char* name)
{
    if (!outer)
        return nullptr;
    void* iter = nullptr;
    while (Il2CppClass* k = api.class_get_nested_types(outer, &iter)) {
        if (std::strcmp(api.class_get_name(k), name) == 0)
            return k;
    }
    SLM_LOG("  missing nested type %s/%s", api.class_get_name(outer), name);
    return nullptr;
}

bool ReadEnum(Il2CppClass* enumClass, std::vector<EnumEntry>& out)
{
    out.clear();
    int size = 0;
    bool isSigned = false;
    if (!EnumUnderlying(enumClass, size, isSigned))
        return false;

    void* iter = nullptr;
    while (FieldInfo* f = api.class_get_fields(enumClass, &iter)) {
        const int flags = api.field_get_flags(f);
        if ((flags & kFieldStatic) == 0 || (flags & kFieldLiteral) == 0)
            continue;
        uint8_t bytes[8] = {};
        api.field_static_get_value(f, bytes);
        out.push_back({api.field_get_name(f), EnumValueFromBytes(bytes, size, isSigned)});
        if (out.size() > 4096)
            return false;
    }
    return !out.empty();
}

bool EnumValue(const std::vector<EnumEntry>& entries, const char* name, int64_t& value)
{
    for (const EnumEntry& e : entries) {
        if (e.name == name) {
            value = e.value;
            return true;
        }
    }
    return false;
}

const char* EnumName(const std::vector<EnumEntry>& entries, int64_t value)
{
    for (const EnumEntry& e : entries) {
        if (e.value == value)
            return e.name.c_str();
    }
    return nullptr;
}

bool ValidateLayouts(const Il2CppImage* core, std::string& problems)
{
    auto check = [&](const char* type, std::initializer_list<std::pair<const char*, size_t>> fields, int32_t size) {
        Il2CppClass* k = api.class_from_name(core, "UnityEngine", type);
        if (!k) {
            problems += std::string("UnityEngine.") + type + " missing; ";
            return false;
        }
        uint32_t align = 0;
        if (api.class_value_size(k, &align) != size) {
            problems += std::string(type) + " size mismatch; ";
            return false;
        }
        size_t first = 0;
        if (!FieldOffset(k, fields.begin()->first, first)) {
            problems += std::string(type) + " first field missing; ";
            return false;
        }
        if (g_valueFieldBase == static_cast<size_t>(-1))
            g_valueFieldBase = first;
        if (first != g_valueFieldBase) {
            problems += std::string(type) + " base offset differs; ";
            return false;
        }
        for (const auto& [name, expected] : fields) {
            size_t off = 0;
            if (!FieldOffset(k, name, off) || off != g_valueFieldBase + expected) {
                problems += std::string(type) + "." + name + " offset mismatch; ";
                return false;
            }
        }
        return true;
    };

    bool ok = true;
    ok &= check("Vector3", {{"x", 0}, {"y", 4}, {"z", 8}}, 12);
    ok &= check("Vector2", {{"x", 0}, {"y", 4}}, 8);
    ok &= check("Rect", {{"m_XMin", 0}, {"m_YMin", 4}, {"m_Width", 8}, {"m_Height", 12}}, 16);
    ok &= check("Color", {{"r", 0}, {"g", 4}, {"b", 8}, {"a", 12}}, 16);
    ok &= check("Matrix4x4", {{"m00", 0}, {"m10", 4}, {"m20", 8}, {"m30", 12}, {"m01", 16}, {"m23", 56}, {"m33", 60}}, 64);
    if (ok && g_valueFieldBase != 0 && g_valueFieldBase != 16) {
        problems += "unexpected value-type field base offset; ";
        ok = false;
    }
    return ok;
}

size_t ValueFieldBase()
{
    return g_valueFieldBase;
}

void SetGameAssemblyRange(void* module)
{
    MODULEINFO info = {};
    if (GetModuleInformation(GetCurrentProcess(), static_cast<HMODULE>(module), &info, sizeof(info))) {
        g_gaBegin = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
        g_gaEnd = g_gaBegin + info.SizeOfImage;
    }
}

bool InGameAssembly(const void* p)
{
    const auto a = reinterpret_cast<uintptr_t>(p);
    return g_gaBegin != 0 && a >= g_gaBegin && a < g_gaEnd;
}

void* MethodPointer(const MethodInfo* m)
{
    if (!m)
        return nullptr;
    void* p = nullptr;
    std::memcpy(&p, m, sizeof(p));
    return InGameAssembly(p) ? p : nullptr;
}

} // namespace slm::rt
