#include "boot_diagnostics.h"
#include "startup_selection.h"

#include <Windows.h>
#include <SDL2/SDL.h>

namespace {

int RunFusionBootstrap(int argc, char** argv) {
    Zelda3DUwp_BootLogStart();
    // SDL_WinRTRunApp owns activation. The pinned Mesa SDL2 bridge obtains its
    // window through SDL; no separate libuwp window-reference cache is used.
    Zelda3DUwpStartupSelection selection;
    char* settingsDirectory = SDL_GetPrefPath(nullptr, "soh");
    const auto result = Zelda3DUwp_ReadStartupSelection(settingsDirectory, selection);
    SDL_free(settingsDirectory);
    if (result == Zelda3DUwpSelectionRead::Invalid || result == Zelda3DUwpSelectionRead::Unreadable) {
        Zelda3DUwp_BootLog("startup.selection.invalid-or-unreadable");
        return ZELDA3D_UWP_INVALID_SELECTION;
    }
    Zelda3DUwp_BootLog(result == Zelda3DUwpSelectionRead::Default
        ? "startup.selection.default-oot" : "startup.selection.loaded");
    char* archiveDirectory = SDL_GetPrefPath(nullptr, selection.game == Zelda3DUwpGame::Mm ? "2ship" : "soh");
    const bool readable = Zelda3DUwp_PrivateArchiveReadable(archiveDirectory, selection.game);
    SDL_free(archiveDirectory);
    if (!readable) {
        Zelda3DUwp_BootLog(selection.game == Zelda3DUwpGame::Mm ? "archive.missing.LocalState-2ship-mm.o2r" :
            (selection.game == Zelda3DUwpGame::OotMq ? "archive.missing.LocalState-soh-oot-mq.o2r" :
                                                    "archive.missing.LocalState-soh-oot.o2r"));
        return ZELDA3D_UWP_INVALID_SELECTION;
    }
    // One core per process launch. Routing is not MQ layout or seed enforcement.
    Zelda3DUwp_BootLog("diagnostic.quest-layout-enforced=false.handoff=false");
    return Zelda3DUwp_RunPackagedCore(selection.pair, selection.game, argc, argv);
}

} // namespace

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return SDL_WinRTRunApp(RunFusionBootstrap, nullptr);
}
