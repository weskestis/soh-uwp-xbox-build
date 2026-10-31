#pragma once

#include <ship/zelda3d_core.h>

// A selected content pair has two games, but MQ uses the same engine as OoT.
// This controls DLL routing only. Seed/save identity and dungeon-layout policy
// must also be enforced by the future combined-game session owner.
enum class Zelda3DUwpPair { OotMm, OotMqMm };
enum class Zelda3DUwpGame { Oot, OotMq, Mm };

struct Zelda3DUwpCoreSpec {
    const char* id;
    const wchar_t* filename;
    const char* logName;
};

// Returns null for an unknown value or a game outside the selected pair.
const Zelda3DUwpCoreSpec* Zelda3DUwp_FindCore(Zelda3DUwpPair pair, Zelda3DUwpGame game);

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
    ZELDA3D_UWP_INVALID_SELECTION = 78,
};

int Zelda3DUwp_RunCore(Zelda3DUwpPair pair, Zelda3DUwpGame game,
                     Zelda3DCoreEntryFn entry, int argc, char** argv);
