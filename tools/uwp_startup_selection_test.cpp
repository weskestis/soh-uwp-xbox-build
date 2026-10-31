#include "startup_selection.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    assert(argc == 2);
    Zelda3DUwpStartupSelection selection;
    for (const auto* text : {"oot-mm oot", "oot-mm mm", "oot-mq-mm oot-mq", "oot-mq-mm mm"}) {
        assert(Zelda3DUwp_ParseStartupSelection(text, selection));
        assert(Zelda3DUwp_FindCore(selection.pair, selection.game) != nullptr);
        assert(Zelda3DUwp_ParseStartupSelection(std::string("\xEF\xBB\xBF \t") + text + "\r\n", selection));
    }
    assert(selection.pair == Zelda3DUwpPair::OotMqMm && selection.game == Zelda3DUwpGame::Mm);
    for (const auto* text : {"", " ", "oot-mm oot-mq", "oot-mq-mm oot", "oot-mm oot mm",
                            "oot-mm ../mm_core.dll", "mm-oot oot", "oot-mm mm\noot-mm oot"}) {
        assert(!Zelda3DUwp_ParseStartupSelection(text, selection));
    }
    assert(!Zelda3DUwp_ParseStartupSelection(std::string(65, 'x'), selection));
    assert(!Zelda3DUwp_ParseStartupSelection(std::string("oot-mm mm\0x", 11), selection));
    const std::filesystem::path directory(argv[1]);
    const auto root = directory.string();
    const auto file = directory / "fusion-startup.txt";
    assert(Zelda3DUwp_ReadStartupSelection(root.c_str(), selection) == Zelda3DUwpSelectionRead::Default);
    assert(selection.game == Zelda3DUwpGame::Oot);
    { std::ofstream out(file); out << "oot-mm mm\n"; }
    assert(Zelda3DUwp_ReadStartupSelection(root.c_str(), selection) == Zelda3DUwpSelectionRead::Loaded);
    assert(selection.game == Zelda3DUwpGame::Mm);
    assert(!Zelda3DUwp_PrivateArchiveReadable(root.c_str(), selection.game));
    { std::ofstream out(directory / "mm.o2r"); }
    assert(!Zelda3DUwp_PrivateArchiveReadable(root.c_str(), selection.game));
    { std::ofstream out(directory / "mm.o2r"); out << "test"; }
    assert(Zelda3DUwp_PrivateArchiveReadable(root.c_str(), selection.game));
    assert(!Zelda3DUwp_PrivateArchiveReadable(root.c_str(), Zelda3DUwpGame::Oot));
    { std::ofstream out(file); out << "oot-mm oot-mq"; }
    assert(Zelda3DUwp_ReadStartupSelection(root.c_str(), selection) == Zelda3DUwpSelectionRead::Invalid);
    { std::ofstream out(file); out << std::string(1000, 'x'); }
    assert(Zelda3DUwp_ReadStartupSelection(root.c_str(), selection) == Zelda3DUwpSelectionRead::Invalid);
    assert(Zelda3DUwp_ReadStartupSelection(nullptr, selection) == Zelda3DUwpSelectionRead::Unreadable);
    assert(Zelda3DUwp_ReadStartupSelection("", selection) == Zelda3DUwpSelectionRead::Unreadable);
    assert(!Zelda3DUwp_PrivateArchiveReadable(nullptr, Zelda3DUwpGame::Oot));
    return 0;
}
