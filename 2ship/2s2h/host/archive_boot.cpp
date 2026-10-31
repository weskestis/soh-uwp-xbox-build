// Archive validation and writable setup shared by desktop and the Xbox core.
#include "host/archive_boot.h"
#include "host/archive_version.h"
#include "BenPort.h"
#include "build.h"
#include "port/core_boot_error.h"

#include <ship/resource/archive/O2rArchive.h>
#include <spdlog/spdlog.h>
#include <exception>
#include <filesystem>
#include <fstream>

namespace MmArchiveBoot {
namespace {

Version CurrentVersion() {
    return { gBuildVersionMajor, gBuildVersionMinor, gBuildVersionPatch };
}

std::optional<Version> ReadVersion(const std::string& path) {
    try {
        if (path.empty() || !std::filesystem::is_regular_file(path)) {
            return std::nullopt;
        }
        Ship::O2rArchive archive(path);
        if (!archive.Open()) {
            return std::nullopt;
        }
        const auto file = archive.LoadFile("portVersion");
        if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
            return std::nullopt;
        }
        return DecodeVersion(reinterpret_cast<const uint8_t*>(file->Buffer->data()), file->Buffer->size());
    } catch (const std::exception& error) {
        SPDLOG_ERROR("Cannot read MM archive '{}': {}", path, error.what());
        return std::nullopt;
    }
}

std::string RomPath() {
    return Ship::Context::LocateFileAcrossAppDirs("mm.o2r", appShortName);
}

#ifdef ZELDA3D_MM_NO_ROM_EXTRACTION
void RequireRomArchive() {
    if (!RomCompatible(ReadVersion(RomPath()), CurrentVersion())) {
        throw Zelda3D::CoreBootError(
            "Missing, corrupt, or incompatible mm.o2r. Import a supported MM archive into this "
            "Fusion package's LocalState/2ship folder. On-console ROM extraction is not implemented yet.");
    }
}
#endif

} // namespace

PortArchive PreparePortArchive() {
    const auto path = Ship::Context::LocateFileAcrossAppDirs("2ship.o2r", appShortName);
    const bool compatible = PortCompatible(ReadVersion(path), CurrentVersion());
#ifdef ZELDA3D_MM_NO_ROM_EXTRACTION
    if (!compatible) {
        throw Zelda3D::CoreBootError(
            "The packaged 2ship.o2r is missing, corrupt, or does not match this MM engine version.");
    }
    RequireRomArchive();
#endif
    return { path, compatible };
}

bool NeedsRomRegeneration() {
    const auto path = RomPath();
    return std::filesystem::exists(path) && !RomCompatible(ReadVersion(path), CurrentVersion());
}

void CreateModFolder() {
    try {
        const auto modsPath = Ship::Context::GetPathRelativeToAppDirectory("mods", appShortName);
        if (std::filesystem::create_directories(modsPath)) {
            std::ofstream(modsPath + "/custom_mod_files_go_here.txt").close();
        }
    } catch (const std::filesystem::filesystem_error& error) {
        SPDLOG_WARN("Could not create MM mods folder: {}", error.what());
    }
}

} // namespace MmArchiveBoot

#ifdef ZELDA3D_MM_NO_ROM_EXTRACTION
void OTRGlobals::RunExtract(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // PreparePortArchive already validated both files before the window was
    // constructed. Do not enter the desktop extraction/permission-probe loop.
    MmArchiveBoot::CreateModFolder();
}
#endif
