#include "boot_diagnostics.h"

#include <Windows.h>
#include <SDL2/SDL.h>

// libuwp captures the CoreWindow reference once on the WinRT UI thread.  SDL2's
// UWP backend and the Mesa/OpenGL bridge then reuse it from the game thread.
extern "C" __declspec(dllimport) void* uwp_GetWindowReference();

namespace {

int RunOotCore(int argc, char** argv) {
    Zelda3DUwp_BootLogStart();
    Zelda3DUwp_BootLog("window-reference.begin");
    uwp_GetWindowReference();
    Zelda3DUwp_BootLog("window-reference.ok");
    return Zelda3DUwp_RunPackagedCore(argc, argv);
}

} // namespace

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return SDL_WinRTRunApp(RunOotCore, nullptr);
}
