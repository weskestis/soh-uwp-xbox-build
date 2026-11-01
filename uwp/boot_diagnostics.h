#pragma once

#include "core_bootstrap.h"

// Own the Xbox package's earliest durable diagnostics and the delayed core-DLL
// handoff.  The log is written to LocalState\soh\uwp-boot.log, beside the
// owner-supplied game files.
void Zelda3DUwp_BootLogStart();
void Zelda3DUwp_BootLog(const char* stage);
int Zelda3DUwp_RunCoreProtected(Zelda3DUwpPair pair, Zelda3DUwpGame game,
                              Zelda3DCoreEntryFn entry, int argc, char** argv);
int Zelda3DUwp_RunPackagedCore(Zelda3DUwpPair pair, Zelda3DUwpGame game,
                             int argc, char** argv);
