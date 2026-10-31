#include "boot_diagnostics.h"

#include <Windows.h>
#include <SDL2/SDL.h>

namespace {

int RunFusionBootstrap(int argc, char** argv) {
    Zelda3DUwp_BootLogStart();
    // SDL_WinRTRunApp owns activation. The pinned Mesa SDL2 bridge obtains its
    // window through SDL; no separate libuwp window-reference cache is used.
    // Keep the existing single-core entry path until MM's UWP build and shared
    // engine ownership are implemented. The pair-aware loader does not itself
    // enforce MQ layouts or provide the combined randomizer/session.
    return Zelda3DUwp_RunPackagedCore(Zelda3DUwpPair::OotMm, Zelda3DUwpGame::Oot, argc, argv);
}

} // namespace

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return SDL_WinRTRunApp(RunFusionBootstrap, nullptr);
}
