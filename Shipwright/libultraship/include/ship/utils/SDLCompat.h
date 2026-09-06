#pragma once

// One include boundary for the two supported SDL ABIs. Desktop builds use SDL3; the opt-in
// Xbox/UWP runtime uses the SDL2/libuwp build that already ships with the V3 package.  The aliases
// below deliberately cover only source-compatible controller names. APIs whose signatures or
// semantics changed (window creation, audio streams, event payloads) stay explicit at their call
// sites so a successful compile cannot hide an ABI mistake.
#ifdef ZELDA3D_USE_SDL2
#include <SDL.h>

using SDL_Gamepad = SDL_GameController;
using SDL_GamepadAxis = SDL_GameControllerAxis;
using SDL_GamepadButton = SDL_GameControllerButton;
using SDL_GamepadDeviceEvent = SDL_ControllerDeviceEvent;

#define SDL_GAMEPAD_AXIS_LEFTX SDL_CONTROLLER_AXIS_LEFTX
#define SDL_GAMEPAD_AXIS_LEFTY SDL_CONTROLLER_AXIS_LEFTY
#define SDL_GAMEPAD_AXIS_RIGHTX SDL_CONTROLLER_AXIS_RIGHTX
#define SDL_GAMEPAD_AXIS_RIGHTY SDL_CONTROLLER_AXIS_RIGHTY
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERRIGHT
#define SDL_GAMEPAD_AXIS_COUNT SDL_CONTROLLER_AXIS_MAX

#define SDL_GAMEPAD_BUTTON_SOUTH SDL_CONTROLLER_BUTTON_A
#define SDL_GAMEPAD_BUTTON_EAST SDL_CONTROLLER_BUTTON_B
#define SDL_GAMEPAD_BUTTON_WEST SDL_CONTROLLER_BUTTON_X
#define SDL_GAMEPAD_BUTTON_NORTH SDL_CONTROLLER_BUTTON_Y
#define SDL_GAMEPAD_BUTTON_BACK SDL_CONTROLLER_BUTTON_BACK
#define SDL_GAMEPAD_BUTTON_GUIDE SDL_CONTROLLER_BUTTON_GUIDE
#define SDL_GAMEPAD_BUTTON_START SDL_CONTROLLER_BUTTON_START
#define SDL_GAMEPAD_BUTTON_LEFT_STICK SDL_CONTROLLER_BUTTON_LEFTSTICK
#define SDL_GAMEPAD_BUTTON_RIGHT_STICK SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define SDL_GAMEPAD_BUTTON_LEFT_SHOULDER SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define SDL_GAMEPAD_BUTTON_DPAD_UP SDL_CONTROLLER_BUTTON_DPAD_UP
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define SDL_GAMEPAD_BUTTON_MISC1 SDL_CONTROLLER_BUTTON_MISC1
#define SDL_GAMEPAD_BUTTON_LEFT_PADDLE1 SDL_CONTROLLER_BUTTON_PADDLE1
#define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 SDL_CONTROLLER_BUTTON_PADDLE2
#define SDL_GAMEPAD_BUTTON_LEFT_PADDLE2 SDL_CONTROLLER_BUTTON_PADDLE3
#define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2 SDL_CONTROLLER_BUTTON_PADDLE4
#define SDL_GAMEPAD_BUTTON_COUNT SDL_CONTROLLER_BUTTON_MAX
#define SDL_INIT_GAMEPAD SDL_INIT_GAMECONTROLLER

#define SDL_GetGamepadAxis SDL_GameControllerGetAxis
#define SDL_GetGamepadButton SDL_GameControllerGetButton
#define SDL_GetGamepadJoystick SDL_GameControllerGetJoystick
#define SDL_GetGamepadName SDL_GameControllerName
#define SDL_CloseGamepad SDL_GameControllerClose
#define SDL_GamepadConnected SDL_GameControllerGetAttached
#define SDL_GamepadHasSensor SDL_GameControllerHasSensor
#define SDL_GetGamepadSensorData SDL_GameControllerGetSensorData
#define SDL_RumbleGamepad SDL_GameControllerRumble
#define SDL_AddGamepadMappingsFromFile SDL_GameControllerAddMappingsFromFile

#define SDL_EVENT_GAMEPAD_ADDED SDL_CONTROLLERDEVICEADDED
#define SDL_EVENT_GAMEPAD_REMOVED SDL_CONTROLLERDEVICEREMOVED
#define SDL_EVENT_GAMEPAD_AXIS_MOTION SDL_CONTROLLERAXISMOTION
#define SDL_EVENT_GAMEPAD_BUTTON_DOWN SDL_CONTROLLERBUTTONDOWN
#define SDL_EVENT_GAMEPAD_BUTTON_UP SDL_CONTROLLERBUTTONUP
#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
#define SDL_EVENT_KEY_UP SDL_KEYUP
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#define SDL_EVENT_TEXT_INPUT SDL_TEXTINPUT
#define SDL_EVENT_FINGER_DOWN SDL_FINGERDOWN
#define SDL_EVENT_FINGER_UP SDL_FINGERUP
#define SDL_EVENT_FINGER_MOTION SDL_FINGERMOTION
#define SDL_EVENT_FIRST SDL_FIRSTEVENT
#define SDL_EVENT_LAST SDL_LASTEVENT

// SDL_Event renamed its controller union members in SDL3. These tokens only occur as member names
// in the first-party event paths and let the shared filtering code remain identical.
#define gdevice cdevice
#define gbutton cbutton
#define gaxis caxis

#define SDL_KMOD_NONE KMOD_NONE
#define SDL_KMOD_ALT KMOD_ALT
#define SDL_KMOD_CAPS KMOD_CAPS
#define SDL_KMOD_CTRL KMOD_CTRL
#define SDL_KMOD_NUM KMOD_NUM
#define SDL_KMOD_SHIFT KMOD_SHIFT

#else
#include <SDL3/SDL.h>
#endif

inline bool Zelda3D_SDLGamepadHasLED(SDL_Gamepad* gamepad) {
#ifdef ZELDA3D_USE_SDL2
    return SDL_GameControllerHasLED(gamepad) == SDL_TRUE;
#else
    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(gamepad), SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
#endif
}

inline bool Zelda3D_SDLGamepadHasRumble(SDL_Gamepad* gamepad) {
#ifdef ZELDA3D_USE_SDL2
    return SDL_GameControllerHasRumble(gamepad) == SDL_TRUE;
#else
    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(gamepad), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
#endif
}

inline bool Zelda3D_SDLSetGamepadSensorEnabled(SDL_Gamepad* gamepad, SDL_SensorType sensorType, bool enabled) {
#ifdef ZELDA3D_USE_SDL2
    return SDL_GameControllerSetSensorEnabled(gamepad, sensorType, enabled ? SDL_TRUE : SDL_FALSE) == 0;
#else
    return SDL_SetGamepadSensorEnabled(gamepad, sensorType, enabled);
#endif
}

inline bool Zelda3D_SDLSetGamepadLED(SDL_Gamepad* gamepad, Uint8 red, Uint8 green, Uint8 blue) {
#ifdef ZELDA3D_USE_SDL2
    return SDL_GameControllerSetLED(gamepad, red, green, blue) == 0;
#else
    return SDL_SetJoystickLED(SDL_GetGamepadJoystick(gamepad), red, green, blue);
#endif
}

inline void Zelda3D_SDLSetMessageBoxButtonId(SDL_MessageBoxButtonData& button, int id) {
#ifdef ZELDA3D_USE_SDL2
    button.buttonid = id;
#else
    button.buttonID = id;
#endif
}
