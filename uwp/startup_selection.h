#pragma once

#include "core_bootstrap.h"
#include <string_view>

struct Zelda3DUwpStartupSelection {
    Zelda3DUwpPair pair = Zelda3DUwpPair::OotMm;
    Zelda3DUwpGame game = Zelda3DUwpGame::Oot;
};

enum class Zelda3DUwpSelectionRead { Default, Loaded, Invalid, Unreadable };

bool Zelda3DUwp_ParseStartupSelection(std::string_view text, Zelda3DUwpStartupSelection& selection);
Zelda3DUwpSelectionRead Zelda3DUwp_ReadStartupSelection(
    const char* directory, Zelda3DUwpStartupSelection& selection);
bool Zelda3DUwp_PrivateArchiveReadable(const char* directory, Zelda3DUwpGame game);
