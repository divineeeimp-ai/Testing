#pragma once

namespace slm::menu {

// Keyboard input, polled on every OnGUI call (edge-detected, only while the game window has focus).
void HandleInput();
// Draws the menu panel; call during Repaint inside gui::BeginFrame/EndFrame.
void Draw();
// Set by the menu's Unload entry or the unload key; consumed by dllmain.
bool UnloadRequested();

} // namespace slm::menu
