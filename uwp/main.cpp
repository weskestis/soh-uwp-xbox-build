#include "boot_diagnostics.h"

#include <Windows.h>
#include <SDL2/SDL.h>

// libuwp captures the CoreWindow reference once on the WinRT UI thread.  SDL2's
// UWP backend and the Mesa/OpenGL bridge then reuse it from the game thread.
extern "C" __declspec(dllimport) void* uwp_GetWindowReference();

namespace {

int RunFusionBootstrap(int argc, char** argv) {
    Zelda3DUwp_BootLogStart();
    Zelda3DUwp_BootLog("window-reference.begin");
    uwp_GetWindowReference();
    Zelda3DUwp_BootLog("window-reference.ok");
    // Keep the existing single-core entry path until MM's UWP build and shared
    // engine ownership are implemented. The pair-aware loader does not itself
    // enforce MQ layouts or provide the combined randomizer/session.
    return Zelda3DUwp_RunPackagedCore(Zelda3DUwpPair::OotMm, Zelda3DUwpGame::Oot, argc, argv);
}

} // namespace

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return SDL_WinRTRunApp(RunFusionBootstrap, nullptr);
}
