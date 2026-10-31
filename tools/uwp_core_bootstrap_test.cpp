#include "core_bootstrap.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <initializer_list>

namespace {

using Pair = Zelda3DUwpPair;
using Game = Zelda3DUwpGame;

int gSeenArgc = -1;
char** gSeenArgv = nullptr;
int gEntryCalls = 0;
int gRunCalls = 0;

int RunOk(int argc, char** argv) {
    ++gRunCalls;
    gSeenArgc = argc;
    gSeenArgv = argv;
    return 23;
}

Zelda3DCore gCore{ ZELDA3D_CORE_ABI, "oot", "Ocarina of Time", RunOk };

const Zelda3DCore* Entry() {
    ++gEntryCalls;
    return &gCore;
}

const Zelda3DCore* NullEntry() {
    ++gEntryCalls;
    return nullptr;
}

} // namespace

int main() {
    char app[] = "uwp-test";
    char arg[] = "--smoke";
    char* argv[] = { app, arg, nullptr };

    // Both allowed content pairs, with the exact DLL/id expectation spelled
    // independently of the implementation. MQ must use OoT's core, not a third.
    const struct {
        Pair pair;
        Game game;
        const char* id;
        const wchar_t* filename;
        const char* logName;
    } valid[] = {
        { Pair::OotMm, Game::Oot, "oot", L"soh_core.dll", "soh_core.dll" },
        { Pair::OotMm, Game::Mm, "mm", L"mm_core.dll", "mm_core.dll" },
        { Pair::OotMqMm, Game::OotMq, "oot", L"soh_core.dll", "soh_core.dll" },
        { Pair::OotMqMm, Game::Mm, "mm", L"mm_core.dll", "mm_core.dll" },
    };
    for (const auto& selection : valid) {
        const auto* spec = Zelda3DUwp_FindCore(selection.pair, selection.game);
        assert(spec != nullptr);
        assert(std::strcmp(spec->id, selection.id) == 0);
        assert(std::wcscmp(spec->filename, selection.filename) == 0);
        assert(std::strcmp(spec->logName, selection.logName) == 0);

        gCore.id = selection.id;
        gCore.abi = ZELDA3D_CORE_ABI;
        gCore.run = RunOk;
        const int runsBefore = gRunCalls;
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, Entry, 2, argv) == 23);
        assert(gRunCalls == runsBefore + 1);
        assert(gSeenArgc == 2 && gSeenArgv == argv);

        // A stale/wrong DLL, missing descriptor, or failed validation must never
        // execute the runner. Exercise each failure for each allowed selection.
        const int runsAfter = gRunCalls;
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, nullptr, 0, nullptr)
               == ZELDA3D_UWP_MISSING_ENTRY);
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, NullEntry, 0, nullptr)
               == ZELDA3D_UWP_NULL_CORE);
        gCore.abi = ZELDA3D_CORE_ABI + 1;
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, Entry, 0, nullptr)
               == ZELDA3D_UWP_ABI_MISMATCH);
        gCore.abi = ZELDA3D_CORE_ABI;
        for (const char* id : { "oot", "mm", "mq", "oot-mq", "unknown", "" }) {
            if (std::strcmp(id, selection.id) == 0) {
                continue;
            }
            gCore.id = id;
            assert(Zelda3DUwp_RunCore(selection.pair, selection.game, Entry, 0, nullptr)
                   == ZELDA3D_UWP_WRONG_CORE);
        }
        gCore.id = nullptr;
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, Entry, 0, nullptr)
               == ZELDA3D_UWP_WRONG_CORE);
        gCore.id = selection.id;
        gCore.run = nullptr;
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, Entry, 0, nullptr)
               == ZELDA3D_UWP_MISSING_RUNNER);
        assert(gRunCalls == runsAfter);
    }

    // Reject an excluded OoT edition and invalid values before calling entry().
    const struct { Pair pair; Game game; } invalid[] = {
        { Pair::OotMm, Game::OotMq },
        { Pair::OotMqMm, Game::Oot },
        { static_cast<Pair>(-1), Game::Mm },
        { static_cast<Pair>(2), Game::Oot },
        { static_cast<Pair>(2), Game::Mm },
        { Pair::OotMm, static_cast<Game>(-1) },
        { Pair::OotMqMm, static_cast<Game>(3) },
    };
    gCore.id = "oot";
    gCore.run = RunOk;
    const int entriesBefore = gEntryCalls;
    const int runsBefore = gRunCalls;
    for (const auto& selection : invalid) {
        assert(Zelda3DUwp_FindCore(selection.pair, selection.game) == nullptr);
        assert(Zelda3DUwp_RunCore(selection.pair, selection.game, Entry, 0, nullptr)
               == ZELDA3D_UWP_INVALID_SELECTION);
    }
    assert(gEntryCalls == entriesBefore && gRunCalls == runsBefore);

    std::puts("Fusion bootstrap: 4 valid routes, 40 descriptor failures, 7 invalid selections PASS");
    return 0;
}
