#include "texture_pack_runtime.h"

#include "../core/zelda3d_runtime.h"
#include "../model/zelda3d_texture_pack_cache.h"
#include "asset/texpack.h"
#include "ship/Context.h"
#include "soh/cvar_prefixes.h"

#include <libultraship/bridge.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

#define ZELDA3D_TEXTURE_PACK_CVAR CVAR_SETTING("OoT3DTexturePackEnabled")

bool sInitialized = false;
bool sExternalOverride = false;
bool sExternalDisabled = false;
bool sRequestedEnabled = true;
bool sApplyPending = false;
bool sRescanPending = false;
bool sReloadQueued = false;
std::string sInstallDirectory;
std::string sSelectedSource;
std::string sDiscoveryError;
std::string sStatus;

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsDisabledValue(const char* value) {
    if (value == nullptr) {
        return false;
    }
    const std::string normalized = Lower(value);
    return normalized == "0" || normalized == "off" || normalized == "none" || normalized == "false";
}

std::string StablePath(const fs::path& path) {
    std::error_code error;
    fs::path absolute = fs::absolute(path, error);
    if (error) {
        absolute = path;
    }
    return absolute.lexically_normal().string();
}

void AddCandidate(const fs::path& path, std::vector<std::string>& candidates,
                  std::unordered_set<std::string>& seen) {
    std::error_code error;
    if (!fs::is_directory(path, error) &&
        !(fs::is_regular_file(path, error) && Lower(path.extension().string()) == ".zip")) {
        return;
    }
    const std::string stable = StablePath(path);
    if (seen.insert(stable).second) {
        candidates.push_back(stable);
    }
}

void AddRootCandidates(const fs::path& root, std::vector<std::string>& candidates,
                       std::unordered_set<std::string>& seen) {
    std::error_code error;
    if (fs::is_regular_file(root, error)) {
        AddCandidate(root, candidates, seen);
        return;
    }
    if (!fs::is_directory(root, error)) {
        return;
    }

    std::vector<fs::path> children;
    for (fs::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        const fs::path child = it->path();
        std::error_code typeError;
        if (fs::is_directory(child, typeError) ||
            (fs::is_regular_file(child, typeError) && Lower(child.extension().string()) == ".zip")) {
            children.push_back(child);
        }
    }
    std::sort(children.begin(), children.end(), [](const fs::path& lhs, const fs::path& rhs) {
        return Lower(lhs.filename().string()) < Lower(rhs.filename().string());
    });
    for (const fs::path& child : children) {
        AddCandidate(child, candidates, seen);
    }

    // Also accept users who copied pack.json/title-ID folders directly into texture-packs.
    AddCandidate(root, candidates, seen);
}

std::vector<std::string> DiscoverCandidates() {
    std::vector<std::string> candidates;
    std::unordered_set<std::string> seen;
    AddRootCandidates(sInstallDirectory, candidates, seen);
    AddRootCandidates("E:/soh/texture-packs", candidates, seen);
    AddRootCandidates("./texture-packs", candidates, seen);
    AddRootCandidates("./textures", candidates, seen);
    return candidates;
}

void DiscoverAndConfigure() {
    sSelectedSource.clear();
    sDiscoveryError.clear();

    const char* environment = std::getenv("ZELDA3D_TEXPACK");
    sExternalOverride = environment != nullptr && *environment != '\0';
    sExternalDisabled = sExternalOverride && IsDisabledValue(environment);
    if (sExternalOverride) {
        if (sExternalDisabled) {
            Zelda3D::TexPackConfigure("", false);
            Zelda3D::TexPackScan();
            sDiscoveryError = "disabled by ZELDA3D_TEXPACK";
            return;
        }
        sSelectedSource = StablePath(environment);
        Zelda3D::TexPackConfigure(sSelectedSource, true);
        if (!Zelda3D::TexPackScan()) {
            sDiscoveryError = Zelda3D::TexPackGetDetails().error;
        }
        return;
    }

    std::error_code createError;
    fs::create_directories(sInstallDirectory, createError);
    if (createError) {
        sDiscoveryError = "cannot create texture-pack folder: " + createError.message();
    }

    for (const std::string& candidate : DiscoverCandidates()) {
        Zelda3D::TexPackConfigure(candidate, sRequestedEnabled);
        if (Zelda3D::TexPackScan()) {
            sSelectedSource = candidate;
            sDiscoveryError.clear();
            return;
        }
        const Zelda3D::TexPackDetails details = Zelda3D::TexPackGetDetails();
        if (!details.error.empty()) {
            sDiscoveryError = details.error;
        }
    }

    Zelda3D::TexPackConfigure("", sRequestedEnabled);
    Zelda3D::TexPackScan();
    if (sDiscoveryError.empty()) {
        sDiscoveryError = "no compatible OoT3D legacy-hash texture pack installed";
    }
}

void InvalidateDependentCaches() {
    // CPU model texture vectors and their renderer uploads must change as one transaction. HUD and
    // menu-atlas caches observe TexPackGeneration lazily and evict their own Fast3D addresses.
    Zelda3D_InvalidateTexturePackModels();
}

void ApplyNow() {
    if (sRescanPending) {
        DiscoverAndConfigure();
    } else if (!sExternalOverride) {
        Zelda3D::TexPackSetEnabled(sRequestedEnabled);
    }

    const Zelda3D::TexPackDetails details = Zelda3D::TexPackGetDetails();
    sApplyPending = false;
    sRescanPending = false;
    sReloadQueued = false;
    InvalidateDependentCaches();

    std::fprintf(stderr, "[Zelda3D] texture-pack runtime: %s (%llu indexed, source=%s)\n",
                 details.active ? "active" : (details.compatible ? "installed/off" : "unavailable"),
                 static_cast<unsigned long long>(details.indexed),
                 details.sourcePath.empty() ? "none" : details.sourcePath.c_str());
}

void EnsureInitialized() {
    if (!sInitialized) {
        Zelda3D_TexturePackInitialize();
    }
}

} // namespace

extern "C" void Zelda3D_TexturePackInitialize(void) {
    sInstallDirectory = Ship::Context::GetPathRelativeToAppDirectory("texture-packs", "soh");
    const char* environment = std::getenv("ZELDA3D_TEXPACK");
    sExternalOverride = environment != nullptr && *environment != '\0';
    sExternalDisabled = sExternalOverride && IsDisabledValue(environment);
    sRequestedEnabled = sExternalOverride ? !sExternalDisabled
                                         : CVarGetInteger(ZELDA3D_TEXTURE_PACK_CVAR, 1) != 0;
    sApplyPending = false;
    sRescanPending = false;
    sReloadQueued = false;
    DiscoverAndConfigure();
    sInitialized = true;
    InvalidateDependentCaches();
}

extern "C" void Zelda3D_TexturePackResetRunState(void) {
    sInitialized = false;
    sExternalOverride = false;
    sExternalDisabled = false;
    sRequestedEnabled = true;
    sApplyPending = false;
    sRescanPending = false;
    sReloadQueued = false;
    sInstallDirectory.clear();
    sSelectedSource.clear();
    sDiscoveryError.clear();
    sStatus.clear();
    Zelda3D::TexPackConfigure("", false);
    InvalidateDependentCaches();
}

extern "C" void Zelda3D_TexturePackRequestEnabled(int enabled) {
    EnsureInitialized();
    if (sExternalOverride) {
        return;
    }

    sRequestedEnabled = enabled != 0;
    CVarSetInteger(ZELDA3D_TEXTURE_PACK_CVAR, sRequestedEnabled ? 1 : 0);
    sApplyPending = true;

    if (gPlayState == nullptr || !Zelda3D_Enabled()) {
        ApplyNow();
    }
}

extern "C" void Zelda3D_TexturePackRequestRescan(void) {
    EnsureInitialized();
    sApplyPending = true;
    sRescanPending = true;
    if (gPlayState == nullptr || !Zelda3D_Enabled()) {
        ApplyNow();
    }
}

extern "C" void Zelda3D_TexturePackProcessRequest(PlayState* play) {
    if (!sInitialized) {
        return;
    }

    if (!sExternalOverride) {
        const bool configured = CVarGetInteger(ZELDA3D_TEXTURE_PACK_CVAR, 1) != 0;
        if (configured != sRequestedEnabled) {
            Zelda3D_TexturePackRequestEnabled(configured ? 1 : 0);
        }
    }

    if (!sApplyPending) {
        return;
    }
    if (play == nullptr || !Zelda3D_Enabled()) {
        ApplyNow();
        return;
    }
    if (sReloadQueued) {
        return;
    }

    // An unrelated transition commits the pending selection in its destination Play_Init. Starting
    // a competing transition here would risk losing the entrance that gameplay already selected.
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return;
    }

    play->nextEntranceIndex = gSaveContext.entranceIndex;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    sReloadQueued = true;
}

extern "C" void Zelda3D_TexturePackApplyPending(void) {
    if (!sInitialized) {
        return;
    }

    if (!sExternalOverride) {
        const bool configured = CVarGetInteger(ZELDA3D_TEXTURE_PACK_CVAR, 1) != 0;
        if (configured != sRequestedEnabled) {
            sRequestedEnabled = configured;
            sApplyPending = true;
        }
    }
    if (sApplyPending) {
        ApplyNow();
    } else {
        sReloadQueued = false;
    }
}

extern "C" int Zelda3D_TexturePackAvailable(void) {
    EnsureInitialized();
    return Zelda3D::TexPackGetDetails().compatible ? 1 : 0;
}

extern "C" int Zelda3D_TexturePackActive(void) {
    EnsureInitialized();
    return Zelda3D::TexPackGetDetails().active ? 1 : 0;
}

extern "C" int Zelda3D_TexturePackRequestedEnabled(void) {
    EnsureInitialized();
    return sRequestedEnabled ? 1 : 0;
}

extern "C" int Zelda3D_TexturePackSwitchPending(void) {
    return sApplyPending ? 1 : 0;
}

extern "C" int Zelda3D_TexturePackExternalOverride(void) {
    EnsureInitialized();
    return sExternalOverride ? 1 : 0;
}

extern "C" int Zelda3D_TexturePackIsArchive(void) {
    EnsureInitialized();
    return Zelda3D::TexPackGetDetails().archive ? 1 : 0;
}

extern "C" uint64_t Zelda3D_TexturePackIndexedCount(void) {
    EnsureInitialized();
    return Zelda3D::TexPackGetDetails().indexed;
}

extern "C" const char* Zelda3D_TexturePackInstallDirectory(void) {
    EnsureInitialized();
    return sInstallDirectory.c_str();
}

extern "C" const char* Zelda3D_TexturePackSource(void) {
    EnsureInitialized();
    const Zelda3D::TexPackDetails details = Zelda3D::TexPackGetDetails();
    sSelectedSource = details.sourcePath;
    return sSelectedSource.c_str();
}

extern "C" const char* Zelda3D_TexturePackName(void) {
    EnsureInitialized();
    static std::string name;
    name = Zelda3D::TexPackGetDetails().displayName;
    return name.c_str();
}

extern "C" const char* Zelda3D_TexturePackVersion(void) {
    EnsureInitialized();
    static std::string version;
    version = Zelda3D::TexPackGetDetails().version;
    return version.c_str();
}

extern "C" const char* Zelda3D_TexturePackError(void) {
    EnsureInitialized();
    static std::string error;
    error = sDiscoveryError.empty() ? Zelda3D::TexPackGetDetails().error : sDiscoveryError;
    return error.c_str();
}

extern "C" const char* Zelda3D_TexturePackStatus(void) {
    EnsureInitialized();
    const Zelda3D::TexPackDetails details = Zelda3D::TexPackGetDetails();
    if (sApplyPending) {
        sStatus = sRescanPending ? "OoT3D texture pack: rescan pending scene reload"
                                 : "OoT3D texture pack: switch pending scene reload";
    } else if (details.active) {
        sStatus = "OoT3D texture pack: active — " + details.displayName;
    } else if (details.compatible) {
        sStatus = "OoT3D texture pack: installed but disabled — " + details.displayName;
    } else if (sExternalDisabled) {
        sStatus = "OoT3D texture pack: disabled by ZELDA3D_TEXPACK";
    } else {
        const std::string error = sDiscoveryError.empty() ? details.error : sDiscoveryError;
        sStatus = "OoT3D texture pack: unavailable" + (error.empty() ? std::string() : " — " + error);
    }
    if (details.compatible) {
        if (!details.version.empty()) {
            sStatus += " " + details.version;
        }
        sStatus += " (" + std::to_string(details.indexed) + " textures, ";
        sStatus += details.archive ? "ZIP)" : "folder)";
    }
    return sStatus.c_str();
}
