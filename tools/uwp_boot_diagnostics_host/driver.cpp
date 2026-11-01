#include "boot_diagnostics.h"
#include "startup_storage.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
char gDirectory[4096] = {};
}

extern "C" char* SDL_GetPrefPath(const char* organization, const char* application) {
    if (organization != nullptr || application == nullptr || std::strcmp(application, "soh") != 0) return nullptr;
    const size_t size = std::strlen(gDirectory) + 1;
    char* result = static_cast<char*>(std::malloc(size));
    if (result != nullptr) std::memcpy(result, gDirectory, size);
    return result;
}

extern "C" void SDL_free(void* memory) {
    std::free(memory);
}

extern "C" __declspec(dllexport) int FusionRunDiagnosticTest(
    const char* directory, Zelda3DCoreEntryFn entry, int mode) {
    if (directory == nullptr || std::strlen(directory) >= sizeof(gDirectory)) return 10;
    std::memcpy(gDirectory, directory, std::strlen(directory) + 1);
    Zelda3DUwp_BootLogStart();
    std::fputs("driver.stdout.shared-crt\n", stdout);
    std::fputs("driver.stderr.shared-crt\n", stderr);
    if (!Zelda3DUwp_PrepareLogDirectory(directory, Zelda3DUwp_BootLog)) return ZELDA3D_UWP_STORAGE_FAILED;
    const int result = Zelda3DUwp_RunCoreProtected(Zelda3DUwpPair::OotMm, Zelda3DUwpGame::Oot,
                                                entry, mode, nullptr);
    Zelda3DUwp_BootLog("native-test.returned");
    return result;
}
