#include "core_bootstrap.h"

#include <cassert>
#include <cstring>

namespace {

int gSeenArgc = -1;
char** gSeenArgv = nullptr;

int RunOk(int argc, char** argv) {
    gSeenArgc = argc;
    gSeenArgv = argv;
    return 23;
}

Zelda3DCore gCore{ ZELDA3D_CORE_ABI, "oot", "Ocarina of Time", RunOk };

const Zelda3DCore* Entry() {
    return &gCore;
}

const Zelda3DCore* NullEntry() {
    return nullptr;
}

} // namespace

int main() {
    char app[] = "uwp-test";
    char arg[] = "--smoke";
    char* argv[] = { app, arg, nullptr };

    assert(Zelda3DUwp_RunCore(Entry, 2, argv) == 23);
    assert(gSeenArgc == 2 && gSeenArgv == argv);
    assert(Zelda3DUwp_RunCore(nullptr, 0, nullptr) == ZELDA3D_UWP_MISSING_ENTRY);
    assert(Zelda3DUwp_RunCore(NullEntry, 0, nullptr) == ZELDA3D_UWP_NULL_CORE);

    gCore.abi = ZELDA3D_CORE_ABI + 1;
    assert(Zelda3DUwp_RunCore(Entry, 0, nullptr) == ZELDA3D_UWP_ABI_MISMATCH);
    gCore.abi = ZELDA3D_CORE_ABI;

    gCore.id = "mm";
    assert(Zelda3DUwp_RunCore(Entry, 0, nullptr) == ZELDA3D_UWP_WRONG_CORE);
    gCore.id = "oot";

    gCore.run = nullptr;
    assert(Zelda3DUwp_RunCore(Entry, 0, nullptr) == ZELDA3D_UWP_MISSING_RUNNER);
    return 0;
}
