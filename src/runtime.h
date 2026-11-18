#pragma once
// Thin helpers over the il2cpp C API: lookup by name, invoke with managed-exception capture,
// enum reading, arrays, strings, and runtime validation of the few layouts that are read directly.

#include "il2cpp_api.h"
#include "math.h"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace slm::rt {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---- lookup ---------------------------------------------------------------------------------------
const Il2CppImage* FindImage(const char* name); // e.g. "Assembly-CSharp.dll"
Il2CppClass* FindClass(const Il2CppImage* image, const char* ns, const char* name);
std::string TypeName(const Il2CppType* type);
// Exact match on name and on parameter type names as il2cpp_type_get_name prints them
// (e.g. "UnityEngine.Rect", "System.String", "ReferenceHub&"). Searches base classes.
// On failure, logs every overload with that name that was found.
const MethodInfo* FindMethod(Il2CppClass* klass, const char* name, std::initializer_list<const char*> params);
FieldInfo* FindField(Il2CppClass* klass, const char* name); // searches base classes
Il2CppObject* TypeObject(Il2CppClass* klass);              // System.Type for typeof(klass)

// ---- invoke ---------------------------------------------------------------------------------------
// virtualCall resolves abstract / virtual / interface methods against the object's runtime class.
// Throws rt::Error when the method is null or the managed code throws.
Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args = nullptr, bool virtualCall = false);

template <class T>
T InvokeValue(const MethodInfo* m, void* obj, void** args = nullptr, bool virtualCall = false)
{
    Il2CppObject* boxed = Invoke(m, obj, args, virtualCall);
    if (!boxed)
        throw Error("value-returning method returned null");
    return *static_cast<const T*>(il2cpp::api.object_unbox(boxed));
}

// ---- objects --------------------------------------------------------------------------------------
template <class T>
T GetField(Il2CppObject* obj, FieldInfo* field)
{
    T value{};
    il2cpp::api.field_get_value(obj, field, &value);
    return value;
}

// UnityEngine.Object is alive when its native pointer (m_CachedPtr) is set; destroyed objects keep a
// managed wrapper whose m_CachedPtr is 0.
void SetUnityObjectClass(Il2CppClass* unityObject);
bool IsUnityAlive(Il2CppObject* obj);

// Copies the elements of a managed reference-type array. Validates the il2cpp array header first.
bool ArrayElements(Il2CppObject* array, uint32_t maxCount, std::vector<Il2CppObject*>& out);

std::string ToUtf8(Il2CppString* s);
Il2CppString* NewString(const std::string& utf8);

// ---- enums ----------------------------------------------------------------------------------------
struct EnumEntry {
    std::string name;
    int64_t value;
};
bool ReadEnum(Il2CppClass* enumClass, std::vector<EnumEntry>& out);
// Size in bytes and signedness of the enum's underlying type (from its value__ field).
bool EnumUnderlying(Il2CppClass* enumClass, int& size, bool& isSigned);
Il2CppClass* FindNested(Il2CppClass* outer, const char* name);
bool EnumValue(const std::vector<EnumEntry>& entries, const char* name, int64_t& value);
const char* EnumName(const std::vector<EnumEntry>& entries, int64_t value); // nullptr if unknown

// ---- layouts --------------------------------------------------------------------------------------
// Structs passed to / returned from Unity by value. Their layout is checked by ValidateLayouts().
struct URect { float x, y, w, h; };
struct UColor { float r, g, b, a; };
using UVector3 = slm::Vec3;
using UMatrix4x4 = slm::Mat4;

// Checks Vector3, Rect, Color and Matrix4x4 field offsets against the structs above and measures how
// il2cpp reports value-type field offsets (with or without the boxed object header).
bool ValidateLayouts(const Il2CppImage* coreModule, std::string& problems);
size_t ValueFieldBase(); // offset that il2cpp adds to fields of value types (0 or object header size)

// GameAssembly.dll address range, for validating code pointers.
void SetGameAssemblyRange(void* module);
bool InGameAssembly(const void* p);
// MethodInfo::methodPointer (first member of MethodInfo in libil2cpp), validated to lie in GameAssembly.
void* MethodPointer(const MethodInfo* m);

} // namespace slm::rt
