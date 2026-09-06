#include "rom_auto_extraction.h"

#include "app_identity.h"
#if !defined(ZELDA3D_UWP)
#include "extractor/Extract.h"
#endif

#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace {

#define ZELDA3D_BOOT(...)                           \
    do {                                            \
        SPDLOG_INFO("[zelda3d boot] " __VA_ARGS__); \
        if (auto _lg = spdlog::default_logger())    \
            _lg->flush();                           \
    } while (0)

#if !defined(ZELDA3D_UWP)
std::vector<std::filesystem::path> CandidateDirectories(const std::filesystem::path& installPath) {
    std::error_code error;
    std::vector<std::filesystem::path> directories = { std::filesystem::current_path(error) };
    for (std::filesystem::path path = installPath;
         !path.empty() && path != path.root_path() && directories.size() < 8; path = path.parent_path()) {
        directories.push_back(path);
    }
    return directories;
}
#endif

std::vector<std::string> FindCandidateRoms(const std::vector<std::filesystem::path>& directories) {
    std::error_code error;
    std::vector<std::string> roms;
    for (const auto& directory : directories) {
        if (directory.empty() || !std::filesystem::is_directory(directory, error)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            const std::string extension = entry.path().extension().string();
            if (extension != ".z64" && extension != ".n64" && extension != ".v64") {
                continue;
            }
            const std::string absolutePath = std::filesystem::absolute(entry.path(), error).string();
            if (std::find(roms.begin(), roms.end(), absolutePath) == roms.end()) {
                roms.push_back(absolutePath);
            }
        }
    }
    std::sort(roms.begin(), roms.end());
    return roms;
}

bool ArchiveExists(const char* archiveName) {
    const std::string path = Ship::Context::LocateFileAcrossAppDirs(archiveName, kSohAppShortName);
    return !path.empty() && std::filesystem::exists(path);
}

} // namespace

bool Zelda3D_AutoExtractVanillaArchive() {
    bool normalReady = ArchiveExists("oot.o2r");
    bool masterQuestReady = ArchiveExists("oot-mq.o2r");
    if (normalReady && masterQuestReady) {
        ZELDA3D_BOOT("AutoExtract: Normal and Master Quest archives already exist; nothing to extract");
        return true;
    }

#if defined(ZELDA3D_UWP)
    ZELDA3D_BOOT("AutoExtract: UWP is ROM-free; upload oot.o2r and/or oot-mq.o2r to LocalState/soh");
    ZELDA3D_BOOT("AutoExtract: imported archive status: Normal={}, Master Quest={}", normalReady, masterQuestReady);
    return normalReady || masterQuestReady;
#else

    const std::string installPath = Ship::Context::GetAppBundlePath();
    const std::vector<std::filesystem::path> directories = CandidateDirectories(installPath);
    const std::vector<std::string> roms = FindCandidateRoms(directories);

    ZELDA3D_BOOT("AutoExtract: archive status before scan: Normal={}, Master Quest={}", normalReady,
                 masterQuestReady);
    ZELDA3D_BOOT("AutoExtract: scanned {} dir(s), found {} candidate ROM(s)", directories.size(), roms.size());
    for (const std::string& rom : roms) {
        Extractor extractor;
        ZELDA3D_BOOT("AutoExtract: validating ROM '{}'", rom);
        if (!extractor.RunFileStandalone(rom)) {
            ZELDA3D_BOOT("AutoExtract: '{}' is not a supported OoT N64 ROM, skipping", rom);
            continue;
        }

        const bool isMasterQuest = extractor.IsMasterQuest();
        bool& archiveReady = isMasterQuest ? masterQuestReady : normalReady;
        const char* archiveName = isMasterQuest ? "oot-mq.o2r" : "oot.o2r";
        const char* editionName = isMasterQuest ? "Master Quest" : "Normal";
        if (archiveReady) {
            ZELDA3D_BOOT("AutoExtract: {} ROM '{}' is supported, but '{}' already exists; preserving it", editionName,
                         rom, archiveName);
            continue;
        }

        std::atomic<size_t> extracted = 0;
        std::atomic<size_t> total = 0;
        ZELDA3D_BOOT("AutoExtract: extracting {} ROM '{}' via ZAPD -> '{}' (this can take a bit)", editionName, rom,
                     installPath);
        extractor.CallZapd(installPath, installPath, &extracted, &total);
        archiveReady = ArchiveExists(archiveName);
        ZELDA3D_BOOT("AutoExtract: ZAPD finished for {} (isMQ={}); '{}' present={}", editionName, isMasterQuest,
                     archiveName, archiveReady);
        if (normalReady && masterQuestReady) {
            ZELDA3D_BOOT("AutoExtract: both Normal and Master Quest archives are now ready");
            return true;
        }
    }

    ZELDA3D_BOOT("AutoExtract: final archive status: Normal={}, Master Quest={}", normalReady, masterQuestReady);
    return normalReady || masterQuestReady;
#endif
}
