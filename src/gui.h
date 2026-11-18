#pragma once
// Drawing through Unity's IMGUI (UnityEngine.GUI), called by name via il2cpp. Valid only inside OnGUI
// on the main thread. Works with any graphics backend (the game runs on Vulkan).

#include "il2cpp_api.h"
#include "math.h"
#include "runtime.h"

#include <string>

namespace slm::gui {

struct Color {
    float r, g, b, a;
};

constexpr float kLineHeight = 14.0f;

// Per OnGUI call. False when drawing is not possible this call.
bool BeginFrame();
void EndFrame(); // restores GUI.color
float ScreenW();
float ScreenH();

void FillRect(const Rect2& r, const Color& c);
void BoxOutline(const Rect2& r, const Color& c);
void Dot(Vec2 center, float size, const Color& c);
bool CanDrawLines();
void Line(Vec2 a, Vec2 b, const Color& c, float thickness);

// Text with a 1px shadow. centered: x is the centre; otherwise x is the left edge.
void Text(float x, float y, Il2CppString* text, const Color& c, bool centered);
void Text(float x, float y, const std::string& utf8, const Color& c, bool centered);

void Shutdown();

} // namespace slm::gui
