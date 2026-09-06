#pragma once

// Own the Xbox package's earliest durable diagnostics and the delayed core-DLL
// handoff.  The log is written to LocalState\soh\uwp-boot.log, beside the
// owner-supplied game files.
void Zelda3DUwp_BootLogStart();
void Zelda3DUwp_BootLog(const char* stage);
int Zelda3DUwp_RunPackagedCore(int argc, char** argv);
