#pragma once

#include <ship/zelda3d_core.h>

// Keep the UWP wrapper's validation independent of Windows and SDL.  That lets
// the ABI handoff be compiled and tested on every development host while the
// actual WinRT entry point stays in main.cpp.
enum Zelda3DUwpBootstrapResult {
    ZELDA3D_UWP_OK = 0,
    ZELDA3D_UWP_MISSING_ENTRY = 70,
    ZELDA3D_UWP_NULL_CORE = 71,
    ZELDA3D_UWP_ABI_MISMATCH = 72,
    ZELDA3D_UWP_WRONG_CORE = 73,
    ZELDA3D_UWP_MISSING_RUNNER = 74,
    ZELDA3D_UWP_CORE_LOAD_FAILED = 75,
    ZELDA3D_UWP_CORE_SYMBOL_FAILED = 76,
    ZELDA3D_UWP_UNHANDLED_EXCEPTION = 77,
};

int Zelda3DUwp_RunCore(Zelda3DCoreEntryFn entry, int argc, char** argv);
