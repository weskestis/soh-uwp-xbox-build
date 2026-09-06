#include "core_bootstrap.h"

#include <cstdio>
#include <cstring>

int Zelda3DUwp_RunCore(Zelda3DCoreEntryFn entry, int argc, char** argv) {
    if (entry == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: the OoT core entry point is missing.\n");
        return ZELDA3D_UWP_MISSING_ENTRY;
    }

    const Zelda3DCore* core = entry();
    if (core == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: the OoT core returned no descriptor.\n");
        return ZELDA3D_UWP_NULL_CORE;
    }
    if (core->abi != ZELDA3D_CORE_ABI) {
        std::fprintf(stderr, "ZELDA3D UWP: core ABI %d does not match wrapper ABI %d.\n", core->abi,
                     ZELDA3D_CORE_ABI);
        return ZELDA3D_UWP_ABI_MISMATCH;
    }
    if (core->id == nullptr || std::strcmp(core->id, "oot") != 0) {
        std::fprintf(stderr, "ZELDA3D UWP: the package accepts only the OoT core.\n");
        return ZELDA3D_UWP_WRONG_CORE;
    }
    if (core->run == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: the OoT core has no run function.\n");
        return ZELDA3D_UWP_MISSING_RUNNER;
    }

    return core->run(argc, argv);
}
