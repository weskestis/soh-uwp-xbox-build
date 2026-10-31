#include "startup_selection.h"

#include <cerrno>
#include <cstdio>
#include <string>

namespace {
constexpr size_t kMaxSelectionBytes = 64;

FILE* OpenRead(const std::string& path, int& error) {
    FILE* file = nullptr;
#ifdef _WIN32
    error = fopen_s(&file, path.c_str(), "rb");
#else
    file = std::fopen(path.c_str(), "rb");
    error = file == nullptr ? errno : 0;
#endif
    return file;
}

std::string InDirectory(const char* directory, const char* filename) {
    if (directory == nullptr || directory[0] == '\0') {
        return {};
    }
    std::string path(directory);
    if (path.size() > 4096) {
        return {};
    }
    if (path.back() != '/' && path.back() != '\\') {
        path += '/';
    }
    path += filename;
    return path;
}
} // namespace

bool Zelda3DUwp_ParseStartupSelection(std::string_view text, Zelda3DUwpStartupSelection& selection) {
    if (text.empty() || text.size() > kMaxSelectionBytes) {
        return false;
    }
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.remove_prefix(3);
    }
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return false;
    }
    text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    struct Allowed {
        std::string_view text;
        Zelda3DUwpPair pair;
        Zelda3DUwpGame game;
    };
    static constexpr Allowed allowed[] = {
        {"oot-mm oot", Zelda3DUwpPair::OotMm, Zelda3DUwpGame::Oot},
        {"oot-mm mm", Zelda3DUwpPair::OotMm, Zelda3DUwpGame::Mm},
        {"oot-mq-mm oot-mq", Zelda3DUwpPair::OotMqMm, Zelda3DUwpGame::OotMq},
        {"oot-mq-mm mm", Zelda3DUwpPair::OotMqMm, Zelda3DUwpGame::Mm},
    };
    for (const auto& option : allowed) {
        if (text == option.text) {
            selection = {option.pair, option.game};
            return true;
        }
    }
    return false;
}

Zelda3DUwpSelectionRead Zelda3DUwp_ReadStartupSelection(
    const char* directory, Zelda3DUwpStartupSelection& selection) {
    selection = {};
    const auto path = InDirectory(directory, "fusion-startup.txt");
    if (path.empty()) {
        return Zelda3DUwpSelectionRead::Unreadable;
    }
    int error = 0;
    FILE* file = OpenRead(path, error);
    if (file == nullptr) {
        return error == ENOENT ? Zelda3DUwpSelectionRead::Default : Zelda3DUwpSelectionRead::Unreadable;
    }
    char text[kMaxSelectionBytes + 1] = {};
    const size_t size = std::fread(text, 1, sizeof(text), file);
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed) {
        return Zelda3DUwpSelectionRead::Unreadable;
    }
    return Zelda3DUwp_ParseStartupSelection({text, size}, selection)
        ? Zelda3DUwpSelectionRead::Loaded : Zelda3DUwpSelectionRead::Invalid;
}

bool Zelda3DUwp_PrivateArchiveReadable(const char* directory, Zelda3DUwpGame game) {
    const char* filename = game == Zelda3DUwpGame::Mm ? "mm.o2r" :
        (game == Zelda3DUwpGame::OotMq ? "oot-mq.o2r" : "oot.o2r");
    const auto path = InDirectory(directory, filename);
    if (path.empty()) {
        return false;
    }
    int error = 0;
    FILE* file = OpenRead(path, error);
    if (file == nullptr) {
        return false;
    }
    // This is a readable/nonempty preflight. Each core still validates content.
    const bool readable = std::fgetc(file) != EOF;
    std::fclose(file);
    return readable;
}
