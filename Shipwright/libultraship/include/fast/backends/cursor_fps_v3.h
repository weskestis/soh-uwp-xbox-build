#pragma once

#include "ship/utils/SDLCompat.h"

namespace Fast {

/**
 * SDL3-native recreation of the controller cursor layer shipped with the
 * SOH CURSOR FPS V3 SDL2 proxy.
 *
 * The runtime starts with cursor mode off. Holding both stick clicks for
 * 350 ms toggles it. While enabled the right stick drives a mouse cursor and
 * the south face button generates a held-capable left mouse button.
 */
void CursorFpsV3Init(SDL_Window* window);
void CursorFpsV3Shutdown();
void CursorFpsV3Tick();

/** Return true when a raw SDL event belongs exclusively to the cursor layer. */
bool CursorFpsV3ConsumeEvent(const SDL_Event& event);

/**
 * Parity accessors used by every SDL controller polling path. Stick clicks
 * are always hidden; cursor-mode A/right-stick input is hidden from gameplay.
 */
bool CursorFpsV3GetGamepadButton(SDL_Gamepad* gamepad, SDL_GamepadButton button);
Sint16 CursorFpsV3GetGamepadAxis(SDL_Gamepad* gamepad, SDL_GamepadAxis axis);

/** Draw the V3 confirmation toast and crosshair into ImGui's foreground list. */
void CursorFpsV3DrawOverlay();

bool CursorFpsV3IsCursorMode();

} // namespace Fast
