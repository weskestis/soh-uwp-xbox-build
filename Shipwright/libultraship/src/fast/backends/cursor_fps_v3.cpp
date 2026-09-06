#include "fast/backends/cursor_fps_v3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include "ship/Context.h"
#include "ship/window/Window.h"
#include "ship/window/gui/Gui.h"

namespace Fast {
namespace {

constexpr Uint64 kToggleHoldMs = 350;
constexpr Uint64 kToastMs = 1600;
constexpr int kDeadzone = 6500;
constexpr int kAxisRangeAfterDeadzone = 32767 - kDeadzone;
constexpr int kMaxPixelsPerSecond = 1150;

struct CursorFpsV3State {
    SDL_Window* window = nullptr;
    SDL_Gamepad* gamepad = nullptr;
    bool cursorMode = false;
    bool chordTracking = false;
    bool chordLatched = false;
    bool syntheticLeftHeld = false;
    bool previousRelativeMode = false;
    bool previousMouseRectSet = false;
    bool restoreCapturePending = false;
    SDL_Rect previousMouseRect{};
    int cursorX = 0;
    int cursorY = 0;
    Uint64 chordStartMs = 0;
    Uint64 lastMoveMs = 0;
    Uint64 toastDeadlineMs = 0;
};

CursorFpsV3State gState;

bool MenuOwnsPointer() {
    auto* context = Ship::Context::GetRawInstance();
    return context != nullptr && context->GetWindow() != nullptr && context->GetWindow()->GetGui() != nullptr &&
           context->GetWindow()->GetGui()->IsInteractiveMenuOpen();
}

void RestorePriorCaptureIfAllowed() {
    if (!gState.restoreCapturePending || MenuOwnsPointer() || gState.window == nullptr) {
        return;
    }
    if (gState.previousRelativeMode) {
#ifdef ZELDA3D_USE_SDL2
        SDL_SetRelativeMouseMode(SDL_TRUE);
#else
        SDL_SetWindowRelativeMouseMode(gState.window, true);
#endif
        SDL_SetWindowMouseRect(gState.window, gState.previousMouseRectSet ? &gState.previousMouseRect : nullptr);
    }
    gState.restoreCapturePending = false;
}

int WindowWidth() {
    int width = 1920;
    int height = 1080;
    if (gState.window != nullptr) {
        SDL_GetWindowSize(gState.window, &width, &height);
    }
    return width > 0 ? width : 1920;
}

int WindowHeight() {
    int width = 1920;
    int height = 1080;
    if (gState.window != nullptr) {
        SDL_GetWindowSize(gState.window, &width, &height);
    }
    return height > 0 ? height : 1080;
}

void QueueMouseButton(bool down) {
    if (gState.window == nullptr) {
        return;
    }

    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
#ifdef ZELDA3D_USE_SDL2
    event.button.timestamp = SDL_GetTicks();
#else
    event.button.timestamp = SDL_GetTicksNS();
#endif
    event.button.windowID = SDL_GetWindowID(gState.window);
    event.button.which = 0;
    event.button.button = SDL_BUTTON_LEFT;
#ifdef ZELDA3D_USE_SDL2
    event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
#else
    event.button.down = down;
#endif
    event.button.clicks = 1;
    event.button.x = gState.cursorX;
    event.button.y = gState.cursorY;
    if (!SDL_PushEvent(&event)) {
        SPDLOG_WARN("Cursor FPS V3: failed to queue synthetic mouse button: {}", SDL_GetError());
    }
}

void QueueMouseMotion(int oldX, int oldY) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
#ifdef ZELDA3D_USE_SDL2
    event.motion.timestamp = SDL_GetTicks();
#else
    event.motion.timestamp = SDL_GetTicksNS();
#endif
    event.motion.windowID = SDL_GetWindowID(gState.window);
    event.motion.which = 0;
    event.motion.state = gState.syntheticLeftHeld ? SDL_BUTTON_LMASK : 0;
    event.motion.x = gState.cursorX;
    event.motion.y = gState.cursorY;
    event.motion.xrel = gState.cursorX - oldX;
    event.motion.yrel = gState.cursorY - oldY;
    if (!SDL_PushEvent(&event)) {
        SPDLOG_WARN("Cursor FPS V3: failed to queue synthetic mouse motion: {}", SDL_GetError());
    }
}

void ReleaseSyntheticLeft() {
    if (!gState.syntheticLeftHeld) {
        return;
    }
    QueueMouseButton(false);
    gState.syntheticLeftHeld = false;
}

void CloseGamepad() {
    ReleaseSyntheticLeft();
    if (gState.gamepad != nullptr) {
        SDL_CloseGamepad(gState.gamepad);
        gState.gamepad = nullptr;
    }
}

void FindFirstGamepad() {
    if (gState.gamepad != nullptr && SDL_GamepadConnected(gState.gamepad)) {
        return;
    }
    CloseGamepad();

#ifdef ZELDA3D_USE_SDL2
    for (int deviceIndex = 0; deviceIndex < SDL_NumJoysticks() && gState.gamepad == nullptr; ++deviceIndex) {
        if (SDL_IsGameController(deviceIndex)) {
            gState.gamepad = SDL_GameControllerOpen(deviceIndex);
        }
    }
#else
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int index = 0; ids != nullptr && index < count && gState.gamepad == nullptr; ++index) {
        gState.gamepad = SDL_OpenGamepad(ids[index]);
    }
    SDL_free(ids);
#endif
}

int CursorDelta(Sint16 raw, Uint64 dtMs) {
    const int value = static_cast<int>(raw);
    const int magnitude = std::abs(value);
    if (magnitude <= kDeadzone) {
        return 0;
    }

    const int adjusted = value < 0 ? -(magnitude - kDeadzone) : magnitude - kDeadzone;
    const int64_t numerator = static_cast<int64_t>(adjusted) * kMaxPixelsPerSecond * static_cast<int64_t>(dtMs);
    const int64_t denominator = static_cast<int64_t>(kAxisRangeAfterDeadzone) * 1000;
    int delta = static_cast<int>(numerator / denominator);
    if (delta == 0) {
        delta = adjusted < 0 ? -1 : 1;
    }
    return delta;
}

void ToggleCursorMode(Uint64 nowMs) {
    gState.cursorMode = !gState.cursorMode;
    gState.toastDeadlineMs = nowMs + kToastMs;

    if (gState.cursorMode) {
#ifdef ZELDA3D_USE_SDL2
        gState.previousRelativeMode = SDL_GetRelativeMouseMode() == SDL_TRUE;
#else
        gState.previousRelativeMode =
            gState.window != nullptr && SDL_GetWindowRelativeMouseMode(gState.window);
#endif
        const SDL_Rect* mouseRect = gState.window != nullptr ? SDL_GetWindowMouseRect(gState.window) : nullptr;
        gState.previousMouseRectSet = mouseRect != nullptr;
        if (mouseRect != nullptr) {
            gState.previousMouseRect = *mouseRect;
        }
        gState.restoreCapturePending = false;
        gState.lastMoveMs = nowMs;
        if (gState.window != nullptr) {
#ifdef ZELDA3D_USE_SDL2
            SDL_SetRelativeMouseMode(SDL_FALSE);
#else
            SDL_SetWindowRelativeMouseMode(gState.window, false);
#endif
            SDL_SetWindowMouseRect(gState.window, nullptr);
        }
        // This seemingly counterintuitive release is what the V3 proxy does: cursor mode must not
        // retain an earlier cross-window mouse capture made by the FPS camera path.
#ifdef ZELDA3D_USE_SDL2
        SDL_CaptureMouse(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
#else
        SDL_CaptureMouse(false);
        SDL_ShowCursor();
#endif

#ifdef ZELDA3D_USE_SDL2
        int x = 0;
        int y = 0;
#else
        float x = 0.0f;
        float y = 0.0f;
#endif
        SDL_GetMouseState(&x, &y);
        const int width = WindowWidth();
        const int height = WindowHeight();
        if (x < 0.0f || x >= width || y < 0.0f || y >= height) {
            gState.cursorX = width / 2;
            gState.cursorY = height / 2;
        } else {
            // SDL2 exposed integer coordinates; truncate SDL3's floats to retain V3 behaviour.
            gState.cursorX = static_cast<int>(x);
            gState.cursorY = static_cast<int>(y);
        }
        if (gState.window != nullptr) {
            SDL_WarpMouseInWindow(gState.window, gState.cursorX, gState.cursorY);
        }
        if (gState.gamepad != nullptr) {
            SDL_RumbleGamepad(gState.gamepad, 0x5000, 0x5000, 120);
        }
    } else {
        ReleaseSyntheticLeft();
        gState.restoreCapturePending = gState.previousRelativeMode;
        RestorePriorCaptureIfAllowed();
        if (gState.gamepad != nullptr) {
            SDL_RumbleGamepad(gState.gamepad, 0x2800, 0x2800, 70);
        }
    }
}

void UpdateToggleChord(Uint64 nowMs) {
    if (gState.gamepad == nullptr) {
        gState.chordTracking = false;
        gState.chordLatched = false;
        return;
    }

    const bool left = SDL_GetGamepadButton(gState.gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    const bool right = SDL_GetGamepadButton(gState.gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    if (!left || !right) {
        gState.chordTracking = false;
        gState.chordLatched = false;
        return;
    }

    if (!gState.chordTracking) {
        gState.chordTracking = true;
        gState.chordStartMs = nowMs;
        return;
    }
    if (!gState.chordLatched && nowMs - gState.chordStartMs >= kToggleHoldMs) {
        ToggleCursorMode(nowMs);
        gState.chordLatched = true;
    }
}

void UpdateCursor(Uint64 nowMs) {
    if (!gState.cursorMode || gState.gamepad == nullptr || gState.window == nullptr) {
        return;
    }

    // V3 repeats this every active pump so another mouse-state manager cannot time the cursor out.
#ifdef ZELDA3D_USE_SDL2
    SDL_ShowCursor(SDL_ENABLE);
#else
    SDL_ShowCursor();
#endif

    const Uint64 elapsed = gState.lastMoveMs == 0 ? 16 : nowMs - gState.lastMoveMs;
    const Uint64 dtMs = std::clamp<Uint64>(elapsed, 1, 50);
    gState.lastMoveMs = nowMs;

    const int dx = CursorDelta(SDL_GetGamepadAxis(gState.gamepad, SDL_GAMEPAD_AXIS_RIGHTX), dtMs);
    const int dy = CursorDelta(SDL_GetGamepadAxis(gState.gamepad, SDL_GAMEPAD_AXIS_RIGHTY), dtMs);
    if (dx != 0 || dy != 0) {
        const int oldX = gState.cursorX;
        const int oldY = gState.cursorY;
        gState.cursorX = std::clamp(gState.cursorX + dx, 0, WindowWidth() - 1);
        gState.cursorY = std::clamp(gState.cursorY + dy, 0, WindowHeight() - 1);
        SDL_WarpMouseInWindow(gState.window, gState.cursorX, gState.cursorY);
        QueueMouseMotion(oldX, oldY);
    }

    // The proxy samples A after moving, so a motion and A edge on the same pump carry the previous
    // button state in the motion event and then the new state in the button event.
    const bool south = SDL_GetGamepadButton(gState.gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    if (south != gState.syntheticLeftHeld) {
        QueueMouseButton(south);
        gState.syntheticLeftHeld = south;
    }
}

// Five columns, seven rows, bit 0 at the top. Only glyphs used by the exact V3 toast are needed.
struct Glyph {
    char character;
    std::array<uint8_t, 5> columns;
};

constexpr std::array<Glyph, 11> kGlyphs = { {
    { 'C', { 0x3e, 0x41, 0x41, 0x41, 0x22 } },
    { 'U', { 0x3f, 0x40, 0x40, 0x40, 0x3f } },
    { 'R', { 0x7f, 0x09, 0x19, 0x29, 0x46 } },
    { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
    { 'O', { 0x3e, 0x41, 0x41, 0x41, 0x3e } },
    { 'M', { 0x7f, 0x02, 0x0c, 0x02, 0x7f } },
    { 'D', { 0x7f, 0x41, 0x41, 0x41, 0x3e } },
    { 'E', { 0x7f, 0x49, 0x49, 0x49, 0x41 } },
    { 'N', { 0x7f, 0x02, 0x04, 0x08, 0x7f } },
    { 'F', { 0x7f, 0x09, 0x09, 0x09, 0x01 } },
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
} };

const Glyph& FindGlyph(char character) {
    for (const auto& glyph : kGlyphs) {
        if (glyph.character == character) {
            return glyph;
        }
    }
    return kGlyphs.back();
}

void DrawBitmapText(ImDrawList* drawList, float x, float y, const char* text) {
    constexpr float scale = 3.0f;
    constexpr float advance = 18.0f;
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    for (const char* cursor = text; *cursor != '\0'; ++cursor, x += advance) {
        const auto& glyph = FindGlyph(*cursor);
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if ((glyph.columns[column] & (1u << row)) == 0) {
                    continue;
                }
                const ImVec2 min(x + column * scale, y + row * scale);
                drawList->AddRectFilled(min, ImVec2(min.x + scale, min.y + scale), white);
            }
        }
    }
}

} // namespace

void CursorFpsV3Init(SDL_Window* window) {
    CursorFpsV3Shutdown();
    gState = {};
    gState.window = window;
}

void CursorFpsV3Shutdown() {
    CloseGamepad();
    gState = {};
}

void CursorFpsV3Tick() {
    FindFirstGamepad();
    const Uint64 nowMs = SDL_GetTicks();
    UpdateToggleChord(nowMs);
    UpdateCursor(nowMs);
    if (!gState.cursorMode) {
        RestorePriorCaptureIfAllowed();
    }
}

bool CursorFpsV3ConsumeEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        const auto button = static_cast<SDL_GamepadButton>(event.gbutton.button);
        if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK || button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            return true;
        }
        if (gState.cursorMode && button == SDL_GAMEPAD_BUTTON_SOUTH) {
            return true;
        }
    }
    if (gState.cursorMode && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
        return axis == SDL_GAMEPAD_AXIS_RIGHTX || axis == SDL_GAMEPAD_AXIS_RIGHTY;
    }
    return false;
}

bool CursorFpsV3GetGamepadButton(SDL_Gamepad* gamepad, SDL_GamepadButton button) {
    if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK || button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
        return false;
    }
    if (gState.cursorMode && button == SDL_GAMEPAD_BUTTON_SOUTH) {
        return false;
    }
    return SDL_GetGamepadButton(gamepad, button);
}

Sint16 CursorFpsV3GetGamepadAxis(SDL_Gamepad* gamepad, SDL_GamepadAxis axis) {
    if (gState.cursorMode && (axis == SDL_GAMEPAD_AXIS_RIGHTX || axis == SDL_GAMEPAD_AXIS_RIGHTY)) {
        return 0;
    }
    return SDL_GetGamepadAxis(gamepad, axis);
}

void CursorFpsV3DrawOverlay() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    const ImU32 white = IM_COL32(255, 255, 255, 255);

    const Uint64 nowMs = SDL_GetTicks();
    if (nowMs < gState.toastDeadlineMs) {
        const char* message = gState.cursorMode ? "CURSOR MODE ON" : "CURSOR MODE OFF";
        const int width = gState.cursorMode ? 276 : 294;
        // The V3 proxy performs signed integer division here. This differs from floor() by one
        // pixel only when the window is narrower than the toast, but preserving it keeps the
        // overlay geometry exact even at pathological resolutions.
        const int windowWidth = static_cast<int>(ImGui::GetIO().DisplaySize.x);
        const float left = static_cast<float>((windowWidth - width) / 2);
        drawList->AddRectFilled(ImVec2(left, 24.0f), ImVec2(left + static_cast<float>(width), 63.0f), black);
        DrawBitmapText(drawList, left + 12.0f, 33.0f, message);
    }

    if (!gState.cursorMode) {
        return;
    }

    const float x = static_cast<float>(gState.cursorX);
    const float y = static_cast<float>(gState.cursorY);
    drawList->AddRectFilled(ImVec2(x - 7.0f, y - 1.0f), ImVec2(x + 8.0f, y + 2.0f), black);
    drawList->AddRectFilled(ImVec2(x - 1.0f, y - 7.0f), ImVec2(x + 2.0f, y + 8.0f), black);
    drawList->AddRectFilled(ImVec2(x - 5.0f, y), ImVec2(x + 6.0f, y + 1.0f), white);
    drawList->AddRectFilled(ImVec2(x, y - 5.0f), ImVec2(x + 1.0f, y + 6.0f), white);
}

bool CursorFpsV3IsCursorMode() {
    return gState.cursorMode;
}

} // namespace Fast
