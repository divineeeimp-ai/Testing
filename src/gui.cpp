#include "gui.h"

#include "game.h"
#include "log.h"

#include <cmath>

namespace slm::gui {

using il2cpp::api;
using game::refs;

namespace {

Il2CppObject* g_white = nullptr; // Texture2D.whiteTexture, fetched per frame
float g_w = 0.0f;
float g_h = 0.0f;
rt::UColor g_savedColor = {1, 1, 1, 1};
bool g_styleTried = false;
uintptr_t g_styleCentered = 0; // GC handles keep the two GUIStyle copies alive between frames
uintptr_t g_styleLeft = 0;

const Color kShadow = {0.0f, 0.0f, 0.0f, 0.85f};

void SetColor(const Color& c)
{
    rt::UColor u = {c.r, c.g, c.b, c.a};
    void* args[] = {&u};
    rt::Invoke(refs.guiSetColor, nullptr, args);
}

uintptr_t MakeStyle(Il2CppObject* source, int64_t alignment)
{
    Il2CppObject* style = api.object_new(refs.guiStyleClass);
    if (!style)
        return 0;
    void* ctorArgs[] = {source};
    rt::Invoke(refs.styleCtorCopy, style, ctorArgs);
    if (refs.styleSetAlignment && alignment >= 0) {
        int32_t a = static_cast<int32_t>(alignment);
        void* args[] = {&a};
        rt::Invoke(refs.styleSetAlignment, style, args);
    }
    if (refs.styleSetFontSize) {
        int32_t size = 11;
        void* args[] = {&size};
        rt::Invoke(refs.styleSetFontSize, style, args);
    }
    if (refs.styleSetRichText) {
        bool rich = false;
        void* args[] = {&rich};
        rt::Invoke(refs.styleSetRichText, style, args);
    }
    return api.gchandle_new(style, false);
}

void MakeStyles()
{
    if (g_styleTried)
        return;
    g_styleTried = true;
    if (!refs.guiLabelStyle || !refs.guiGetSkin || !refs.skinGetLabel || !refs.styleCtorCopy || !refs.guiStyleClass ||
        !api.gchandle_new || !api.gchandle_get_target || refs.textAnchorUpperCenter < 0) {
        SLM_LOG("GUIStyle copies unavailable: text is left-aligned with estimated centring");
        return;
    }
    try {
        Il2CppObject* skin = rt::Invoke(refs.guiGetSkin, nullptr);
        Il2CppObject* label = skin ? rt::Invoke(refs.skinGetLabel, skin) : nullptr;
        if (!label)
            return;
        g_styleCentered = MakeStyle(label, refs.textAnchorUpperCenter);
        g_styleLeft = MakeStyle(label, refs.textAnchorUpperLeft);
    } catch (const rt::Error& e) {
        SLM_LOG("GUIStyle creation failed: %s", e.what());
        g_styleCentered = g_styleLeft = 0;
    }
}

Il2CppObject* Style(bool centered)
{
    const uintptr_t h = centered ? g_styleCentered : g_styleLeft;
    return h ? api.gchandle_get_target(h) : nullptr;
}

void Label(float x, float y, Il2CppString* text, bool centered)
{
    Il2CppObject* style = Style(centered);
    if (style) {
        rt::URect r = centered ? rt::URect{x - 250.0f, y, 500.0f, 20.0f} : rt::URect{x, y, 600.0f, 20.0f};
        void* args[] = {&r, text, style};
        rt::Invoke(refs.guiLabelStyle, nullptr, args);
        return;
    }
    // No style: default skin label, left-aligned; approximate centring from the character count.
    float left = x;
    if (centered)
        left -= static_cast<float>(api.string_length(text)) * 3.2f;
    rt::URect r = {left, y, 600.0f, 20.0f};
    if (refs.guiLabel) {
        void* args[] = {&r, text};
        rt::Invoke(refs.guiLabel, nullptr, args);
    }
}

} // namespace

bool BeginFrame()
{
    if (!refs.drawing)
        return false;
    g_white = rt::Invoke(refs.texWhite, nullptr);
    g_w = static_cast<float>(rt::InvokeValue<int32_t>(refs.screenWidth, nullptr));
    g_h = static_cast<float>(rt::InvokeValue<int32_t>(refs.screenHeight, nullptr));
    g_savedColor = rt::InvokeValue<rt::UColor>(refs.guiGetColor, nullptr);
    MakeStyles();
    return g_white != nullptr && g_w >= 1.0f && g_h >= 1.0f;
}

void EndFrame()
{
    void* args[] = {&g_savedColor};
    rt::Invoke(refs.guiSetColor, nullptr, args);
}

float ScreenW() { return g_w; }
float ScreenH() { return g_h; }

void FillRect(const Rect2& rect, const Color& c)
{
    SetColor(c);
    rt::URect r = {rect.x, rect.y, rect.w, rect.h};
    void* args[] = {&r, g_white};
    rt::Invoke(refs.guiDrawTexture, nullptr, args);
}

static void Frame(const Rect2& r, const Color& c, float t)
{
    FillRect({r.x, r.y, r.w, t}, c);
    FillRect({r.x, r.y + r.h - t, r.w, t}, c);
    FillRect({r.x, r.y, t, r.h}, c);
    FillRect({r.x + r.w - t, r.y, t, r.h}, c);
}

void BoxOutline(const Rect2& r, const Color& c)
{
    Frame({r.x - 1.0f, r.y - 1.0f, r.w + 2.0f, r.h + 2.0f}, kShadow, 3.0f);
    Frame(r, c, 1.0f);
}

void Dot(Vec2 p, float size, const Color& c)
{
    FillRect({p.x - size * 0.5f - 1.0f, p.y - size * 0.5f - 1.0f, size + 2.0f, size + 2.0f}, kShadow);
    FillRect({p.x - size * 0.5f, p.y - size * 0.5f, size, size}, c);
}

bool CanDrawLines()
{
    return refs.guiGetMatrix && refs.guiSetMatrix && refs.guiRotateAroundPivot;
}

void Line(Vec2 a, Vec2 b, const Color& c, float thickness)
{
    if (!CanDrawLines())
        return;
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (!(length >= 0.5f) || !std::isfinite(length))
        return;
    const float angle = std::atan2(dy, dx) * 57.2957795f;

    rt::UMatrix4x4 saved = rt::InvokeValue<rt::UMatrix4x4>(refs.guiGetMatrix, nullptr);
    float pivot[2] = {a.x, a.y};
    float deg = angle;
    void* rotArgs[] = {&deg, pivot};
    rt::Invoke(refs.guiRotateAroundPivot, nullptr, rotArgs);
    FillRect({a.x, a.y - thickness * 0.5f, length, thickness}, c);
    void* restoreArgs[] = {&saved};
    rt::Invoke(refs.guiSetMatrix, nullptr, restoreArgs);
}

void Text(float x, float y, Il2CppString* text, const Color& c, bool centered)
{
    if (!text || api.string_length(text) <= 0)
        return;
    SetColor(kShadow);
    Label(x + 1.0f, y + 1.0f, text, centered);
    SetColor(c);
    Label(x, y, text, centered);
}

void Text(float x, float y, const std::string& utf8, const Color& c, bool centered)
{
    if (utf8.empty())
        return;
    Text(x, y, rt::NewString(utf8), c, centered);
}

void Shutdown()
{
    if (api.gchandle_free) {
        if (g_styleCentered)
            api.gchandle_free(g_styleCentered);
        if (g_styleLeft)
            api.gchandle_free(g_styleLeft);
    }
    g_styleCentered = g_styleLeft = 0;
}

} // namespace slm::gui
