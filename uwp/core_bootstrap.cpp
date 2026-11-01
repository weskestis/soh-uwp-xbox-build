#include "core_bootstrap.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr Zelda3DUwpCoreSpec kOotCore{ "oot", L"soh_core.dll", "soh_core.dll" };
constexpr Zelda3DUwpCoreSpec kMmCore{ "mm", L"mm_core.dll", "mm_core.dll" };

} // namespace

const Zelda3DUwpCoreSpec* Zelda3DUwp_FindCore(Zelda3DUwpPair pair, Zelda3DUwpGame game) {
    // Keep the allowlist explicit: an invalid enum must never fall through to a
    // default game, and uploading all three ROMs must not create a third pairing.
    switch (pair) {
        case Zelda3DUwpPair::OotMm:
            if (game == Zelda3DUwpGame::Oot) {
                return &kOotCore;
            }
            break;
        case Zelda3DUwpPair::OotMqMm:
            if (game == Zelda3DUwpGame::OotMq) {
                return &kOotCore;
            }
            break;
        default:
            return nullptr;
    }
    return game == Zelda3DUwpGame::Mm ? &kMmCore : nullptr;
}

int Zelda3DUwp_RunCore(Zelda3DUwpPair pair, Zelda3DUwpGame game,
                     Zelda3DCoreEntryFn entry, int argc, char** argv,
                     void (*trace)(const char*)) {
    const Zelda3DUwpCoreSpec* expected = Zelda3DUwp_FindCore(pair, game);
    if (expected == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: game is not in a supported Fusion pair.\n");
        return ZELDA3D_UWP_INVALID_SELECTION;
    }
    if (entry == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: the %s core entry point is missing.\n", expected->id);
        return ZELDA3D_UWP_MISSING_ENTRY;
    }

    if (trace != nullptr) trace("core.descriptor.enter");
    const Zelda3DCore* core = entry();
    if (trace != nullptr) trace("core.descriptor.return");
    if (core == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: the %s core returned no descriptor.\n", expected->id);
        return ZELDA3D_UWP_NULL_CORE;
    }
    if (core->abi != ZELDA3D_CORE_ABI) {
        std::fprintf(stderr, "ZELDA3D UWP: core ABI %d does not match wrapper ABI %d.\n", core->abi,
                     ZELDA3D_CORE_ABI);
        return ZELDA3D_UWP_ABI_MISMATCH;
    }
    if (core->id == nullptr || std::strcmp(core->id, expected->id) != 0) {
        std::fprintf(stderr, "ZELDA3D UWP: expected %s core; loaded %s.\n", expected->id,
                     core->id != nullptr ? core->id : "(null)");
        return ZELDA3D_UWP_WRONG_CORE;
    }
    if (core->run == nullptr) {
        std::fprintf(stderr, "ZELDA3D UWP: the %s core has no run function.\n", expected->id);
        return ZELDA3D_UWP_MISSING_RUNNER;
    }

    if (trace != nullptr) trace("core.descriptor.valid");
    if (trace != nullptr) trace("core.runner.enter");
    const int result = core->run(argc, argv);
    if (trace != nullptr) trace("core.runner.return");
    return result;
}
