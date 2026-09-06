#include "texpack.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <stb_image.h>
#include <zip.h>

namespace fs = std::filesystem;

namespace Zelda3D {

namespace {

constexpr const char* kOot3dUsTitleId = "0004000000033500";
constexpr uint64_t kMaxPackEntries = 100000;
constexpr uint64_t kMaxManifestBytes = 1024 * 1024;
constexpr uint64_t kMaxPngBytes = 256ULL * 1024 * 1024;
constexpr uint64_t kMaxDecodedPixels = 8192ULL * 8192;
constexpr zip_uint64_t kNoZipEntry = std::numeric_limits<zip_uint64_t>::max();

struct TextureLocation {
    std::string diskPath;
    zip_uint64_t zipEntry = kNoZipEntry;
    uint64_t bytes = 0;
};

struct TexPack {
    bool configured = false;
    bool scanned = false;
    bool requestedEnabled = true;
    bool compatible = false;
    bool archive = false;
    bool hasManifest = false;
    bool flipPngFiles = true;
    bool ourFlip = false;
    uint64_t generation = 1;
    uint64_t duplicates = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    std::string configuredSource;
    std::string sourcePath;
    std::string displayName;
    std::string version;
    std::string error;
    std::unordered_map<uint64_t, TextureLocation> index;
    zip_t* zipArchive = nullptr;

    ~TexPack() {
        if (zipArchive != nullptr) {
            zip_close(zipArchive);
        }
    }
};

TexPack g_pack;
std::mutex g_packMutex;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool disabledValue(const char* value) {
    if (value == nullptr) {
        return false;
    }
    const std::string normalized = lower(value);
    return normalized == "0" || normalized == "off" || normalized == "none" || normalized == "false";
}

void clearScanState() {
    if (g_pack.zipArchive != nullptr) {
        zip_close(g_pack.zipArchive);
        g_pack.zipArchive = nullptr;
    }
    g_pack.scanned = false;
    g_pack.compatible = false;
    g_pack.archive = false;
    g_pack.hasManifest = false;
    g_pack.flipPngFiles = true;
    g_pack.ourFlip = false;
    g_pack.duplicates = 0;
    g_pack.hits = 0;
    g_pack.misses = 0;
    g_pack.sourcePath.clear();
    g_pack.displayName.clear();
    g_pack.version.clear();
    g_pack.error.clear();
    g_pack.index.clear();
}

std::string absolutePath(const fs::path& path) {
    std::error_code error;
    const fs::path absolute = fs::absolute(path, error);
    return error ? path.string() : absolute.lexically_normal().string();
}

std::string baseName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Accept current Citra mip naming and older packs that omitted the explicit mip token.
bool parseHash(const std::string& fileName, uint64_t& out) {
    const std::string normalized = lower(fileName);
    if (normalized.rfind("tex1_", 0) != 0 || normalized.size() < 5 ||
        normalized.substr(normalized.size() - 4) != ".png") {
        return false;
    }
    const size_t mip = normalized.rfind("_mip");
    if (mip != std::string::npos && normalized.compare(mip, 9, "_mip0.png") != 0) {
        return false;
    }
    const size_t dimensionsEnd = fileName.find('_', 5);
    if (dimensionsEnd == std::string::npos) {
        return false;
    }
    const size_t hashEnd = fileName.find('_', dimensionsEnd + 1);
    if (hashEnd == std::string::npos) {
        return false;
    }
    const std::string hex = fileName.substr(dimensionsEnd + 1, hashEnd - dimensionsEnd - 1);
    if (hex.size() != 16 || !std::all_of(hex.begin(), hex.end(), [](unsigned char c) { return std::isxdigit(c); })) {
        return false;
    }
    char* end = nullptr;
    out = std::strtoull(hex.c_str(), &end, 16);
    return end != nullptr && *end == '\0';
}

std::optional<bool> jsonBoolean(const std::string& json, const char* field) {
    const std::string key = std::string("\"") + field + "\"";
    const size_t keyAt = json.find(key);
    if (keyAt == std::string::npos) {
        return std::nullopt;
    }
    const size_t colon = json.find(':', keyAt + key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    size_t valueAt = colon + 1;
    while (valueAt < json.size() && std::isspace(static_cast<unsigned char>(json[valueAt]))) {
        valueAt++;
    }
    if (json.compare(valueAt, 4, "true") == 0) {
        return true;
    }
    if (json.compare(valueAt, 5, "false") == 0) {
        return false;
    }
    return std::nullopt;
}

std::string jsonString(const std::string& json, const char* field) {
    const std::string key = std::string("\"") + field + "\"";
    const size_t keyAt = json.find(key);
    if (keyAt == std::string::npos) {
        return {};
    }
    const size_t colon = json.find(':', keyAt + key.size());
    const size_t quote = colon == std::string::npos ? std::string::npos : json.find('"', colon + 1);
    if (quote == std::string::npos) {
        return {};
    }
    std::string result;
    bool escaped = false;
    for (size_t i = quote + 1; i < json.size(); i++) {
        const char c = json[i];
        if (escaped) {
            switch (c) {
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default: result.push_back(c); break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return result;
        } else {
            result.push_back(c);
        }
    }
    return {};
}

void observeTitleId(const std::string& path, bool& sawOot3d, bool& sawForeignTitle) {
    std::string component;
    auto inspect = [&](const std::string& value) {
        if (value.size() != 16 || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return std::isxdigit(c);
            })) {
            return;
        }
        const std::string id = lower(value);
        if (id.rfind("00040000", 0) != 0) {
            return;
        }
        if (id == lower(kOot3dUsTitleId)) {
            sawOot3d = true;
        } else {
            sawForeignTitle = true;
        }
    };
    for (const char c : path) {
        if (c == '/' || c == '\\') {
            inspect(component);
            component.clear();
        } else {
            component.push_back(c);
        }
    }
    inspect(component);
}

bool readDiskFile(const fs::path& path, uint64_t limit, std::vector<uint8_t>& bytes) {
    std::error_code error;
    const uint64_t size = fs::file_size(path, error);
    if (error || size == 0 || size > limit || size > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    bytes.resize(static_cast<size_t>(size));
    return static_cast<bool>(stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)));
}

bool readZipEntry(zip_uint64_t entry, uint64_t size, uint64_t limit, std::vector<uint8_t>& bytes) {
    if (g_pack.zipArchive == nullptr || size == 0 || size > limit ||
        size > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return false;
    }
    zip_file_t* file = zip_fopen_index(g_pack.zipArchive, entry, ZIP_FL_UNCHANGED);
    if (file == nullptr) {
        return false;
    }
    bytes.resize(static_cast<size_t>(size));
    uint64_t consumed = 0;
    while (consumed < size) {
        const zip_int64_t count = zip_fread(file, bytes.data() + consumed, size - consumed);
        if (count <= 0) {
            zip_fclose(file);
            bytes.clear();
            return false;
        }
        consumed += static_cast<uint64_t>(count);
    }
    return zip_fclose(file) == 0;
}

bool readManifestDisk(const fs::path& path, std::string& manifest) {
    std::vector<uint8_t> bytes;
    if (!readDiskFile(path, kMaxManifestBytes, bytes)) {
        return false;
    }
    manifest.assign(bytes.begin(), bytes.end());
    return true;
}

bool readManifestZip(zip_uint64_t entry, uint64_t size, std::string& manifest) {
    std::vector<uint8_t> bytes;
    if (!readZipEntry(entry, size, kMaxManifestBytes, bytes)) {
        return false;
    }
    manifest.assign(bytes.begin(), bytes.end());
    return true;
}

void addTexture(uint64_t hash, TextureLocation location) {
    if (!g_pack.index.emplace(hash, std::move(location)).second) {
        g_pack.duplicates++;
    }
}

bool scanDirectory(const fs::path& root, std::string& manifest, bool& sawOot3d, bool& sawForeignTitle) {
    std::error_code error;
    fs::path firstManifest;
    fs::path ootManifest;
    for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_regular_file(error)) {
            continue;
        }
        const std::string genericPath = it->path().generic_string();
        observeTitleId(genericPath, sawOot3d, sawForeignTitle);
        const std::string fileName = it->path().filename().string();
        if (lower(fileName) == "pack.json") {
            if (firstManifest.empty()) {
                firstManifest = it->path();
            }
            if (genericPath.find(kOot3dUsTitleId) != std::string::npos) {
                ootManifest = it->path();
            }
            continue;
        }
        uint64_t hash = 0;
        if (!parseHash(fileName, hash)) {
            continue;
        }
        const uint64_t size = fs::file_size(it->path(), error);
        if (error) {
            error.clear();
            continue;
        }
        addTexture(hash, TextureLocation{ it->path().string(), kNoZipEntry, size });
        if (g_pack.index.size() > kMaxPackEntries) {
            g_pack.error = "texture pack contains more than 100000 unique textures";
            return false;
        }
    }
    if (error) {
        g_pack.error = "failed while scanning texture-pack directory: " + error.message();
        return false;
    }
    const fs::path selectedManifest = !ootManifest.empty() ? ootManifest : firstManifest;
    if (!selectedManifest.empty()) {
        g_pack.hasManifest = true;
        if (!readManifestDisk(selectedManifest, manifest)) {
            g_pack.error = "pack.json is unreadable or larger than 1 MiB";
            return false;
        }
    }
    return true;
}

bool scanArchive(const fs::path& archivePath, std::string& manifest, bool& sawOot3d, bool& sawForeignTitle) {
    int zipError = 0;
    g_pack.zipArchive = zip_open(archivePath.string().c_str(), ZIP_RDONLY, &zipError);
    if (g_pack.zipArchive == nullptr) {
        char buffer[160];
        zip_error_to_str(buffer, sizeof(buffer), zipError, errno);
        g_pack.error = std::string("cannot open texture-pack ZIP: ") + buffer;
        return false;
    }
    const zip_int64_t count = zip_get_num_entries(g_pack.zipArchive, ZIP_FL_UNCHANGED);
    if (count < 0 || static_cast<uint64_t>(count) > kMaxPackEntries) {
        g_pack.error = "texture-pack ZIP has an invalid or excessive entry count";
        return false;
    }

    zip_uint64_t firstManifest = kNoZipEntry;
    zip_uint64_t ootManifest = kNoZipEntry;
    uint64_t firstManifestSize = 0;
    uint64_t ootManifestSize = 0;
    for (zip_uint64_t entry = 0; entry < static_cast<zip_uint64_t>(count); entry++) {
        zip_stat_t stat{};
        zip_stat_init(&stat);
        if (zip_stat_index(g_pack.zipArchive, entry, ZIP_FL_UNCHANGED, &stat) != 0 || stat.name == nullptr) {
            continue;
        }
        const std::string path = stat.name;
        if (path.empty() || path.back() == '/') {
            continue;
        }
        observeTitleId(path, sawOot3d, sawForeignTitle);
        const std::string fileName = baseName(path);
        if (lower(fileName) == "pack.json") {
            if (firstManifest == kNoZipEntry) {
                firstManifest = entry;
                firstManifestSize = stat.size;
            }
            if (path.find(kOot3dUsTitleId) != std::string::npos) {
                ootManifest = entry;
                ootManifestSize = stat.size;
            }
            continue;
        }
        uint64_t hash = 0;
        if (parseHash(fileName, hash)) {
            addTexture(hash, TextureLocation{ {}, entry, stat.size });
        }
    }
    const zip_uint64_t selectedManifest = ootManifest != kNoZipEntry ? ootManifest : firstManifest;
    const uint64_t selectedManifestSize = ootManifest != kNoZipEntry ? ootManifestSize : firstManifestSize;
    if (selectedManifest != kNoZipEntry) {
        g_pack.hasManifest = true;
        if (!readManifestZip(selectedManifest, selectedManifestSize, manifest)) {
            g_pack.error = "pack.json in ZIP is unreadable or larger than 1 MiB";
            return false;
        }
    }
    return true;
}

bool directoryHasMip0Texture(const fs::path& root) {
    std::error_code error;
    if (!fs::is_directory(root, error)) {
        return false;
    }
    for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        uint64_t hash = 0;
        if (it->is_regular_file(error) && parseHash(it->path().filename().string(), hash)) {
            return true;
        }
    }
    return false;
}

fs::path autoSource() {
    if (const char* environment = std::getenv("ZELDA3D_TEXPACK"); environment != nullptr && *environment != '\0') {
        if (disabledValue(environment)) {
            g_pack.requestedEnabled = false;
            g_pack.error = "disabled by ZELDA3D_TEXPACK";
            return {};
        }
        return environment;
    }
    if (directoryHasMip0Texture("textures")) {
        return "textures";
    }
    if (const char* romPath = std::getenv("ZELDA3D_OOT3D_ROM"); romPath != nullptr && *romPath != '\0') {
        const fs::path candidate = fs::path(romPath).parent_path() / "textures";
        if (directoryHasMip0Texture(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool finalizeScan(const fs::path& source, const std::string& manifest, bool sawOot3d, bool sawForeignTitle) {
    if (g_pack.index.empty()) {
        g_pack.error = "no Citra legacy tex1_* mip-0 PNG files found";
        return false;
    }
    if (sawForeignTitle && !sawOot3d) {
        g_pack.error = std::string("pack targets a different 3DS title; expected ") + kOot3dUsTitleId;
        return false;
    }
    if (g_pack.hasManifest) {
        if (jsonBoolean(manifest, "use_new_hash").value_or(false)) {
            g_pack.error = "pack.json requests use_new_hash=true; this renderer requires Citra legacy hashes";
            return false;
        }
        g_pack.flipPngFiles = jsonBoolean(manifest, "flip_png_files").value_or(true);
        g_pack.displayName = jsonString(manifest, "name");
        if (g_pack.displayName.empty()) {
            g_pack.displayName = jsonString(manifest, "description");
        }
        g_pack.version = jsonString(manifest, "version");
    }
    g_pack.ourFlip = !g_pack.flipPngFiles;
    if (g_pack.displayName.empty()) {
        g_pack.displayName = source.stem().string();
    }
    g_pack.compatible = true;
    return true;
}

bool scanLocked() {
    if (g_pack.scanned) {
        return g_pack.compatible;
    }
    g_pack.scanned = true;

    const fs::path source = g_pack.configured ? fs::path(g_pack.configuredSource) : autoSource();
    if (source.empty()) {
        if (g_pack.error.empty()) {
            g_pack.error = "no texture pack installed";
        }
        std::fprintf(stderr, "[Zelda3D] texpack: %s\n", g_pack.error.c_str());
        return false;
    }
    g_pack.sourcePath = absolutePath(source);

    std::error_code error;
    std::string manifest;
    bool sawOot3d = false;
    bool sawForeignTitle = false;
    bool scanned = false;
    if (fs::is_directory(source, error)) {
        scanned = scanDirectory(source, manifest, sawOot3d, sawForeignTitle);
    } else if (fs::is_regular_file(source, error) && lower(source.extension().string()) == ".zip") {
        g_pack.archive = true;
        scanned = scanArchive(source, manifest, sawOot3d, sawForeignTitle);
    } else {
        g_pack.error = "texture-pack source is neither a directory nor a ZIP: " + g_pack.sourcePath;
    }
    if (scanned) {
        scanned = finalizeScan(source, manifest, sawOot3d, sawForeignTitle);
    }
    if (!scanned) {
        if (g_pack.zipArchive != nullptr) {
            zip_close(g_pack.zipArchive);
            g_pack.zipArchive = nullptr;
        }
        std::fprintf(stderr, "[Zelda3D] texpack: rejected %s (%s)\n", g_pack.sourcePath.c_str(),
                     g_pack.error.c_str());
        return false;
    }

    std::fprintf(stderr, "[Zelda3D] texpack: %zu textures indexed from %s%s%s (zip=%d enabled=%d flip=%d)\n",
                 g_pack.index.size(), g_pack.displayName.c_str(), g_pack.version.empty() ? "" : " ",
                 g_pack.version.c_str(), static_cast<int>(g_pack.archive), static_cast<int>(g_pack.requestedEnabled),
                 static_cast<int>(g_pack.ourFlip));
    return true;
}

bool readTexture(const TextureLocation& location, std::vector<uint8_t>& bytes) {
    if (location.zipEntry != kNoZipEntry) {
        return readZipEntry(location.zipEntry, location.bytes, kMaxPngBytes, bytes);
    }
    return readDiskFile(location.diskPath, kMaxPngBytes, bytes);
}

} // namespace

void TexPackConfigure(const std::string& sourcePath, bool enabled) {
    std::lock_guard<std::mutex> lock(g_packMutex);
    clearScanState();
    g_pack.configured = true;
    g_pack.configuredSource = sourcePath;
    g_pack.requestedEnabled = enabled;
    g_pack.generation++;
}

void TexPackSetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_packMutex);
    if (g_pack.requestedEnabled == enabled) {
        return;
    }
    g_pack.requestedEnabled = enabled;
    g_pack.generation++;
}

void TexPackUseAutoDiscovery() {
    std::lock_guard<std::mutex> lock(g_packMutex);
    clearScanState();
    g_pack.configured = false;
    g_pack.configuredSource.clear();
    g_pack.requestedEnabled = true;
    g_pack.generation++;
}

bool TexPackScan() {
    std::lock_guard<std::mutex> lock(g_packMutex);
    return scanLocked();
}

bool TexPackLookup(uint64_t hash, int& width, int& height, std::vector<uint8_t>& rgba) {
    std::lock_guard<std::mutex> lock(g_packMutex);
    if (!scanLocked() || !g_pack.requestedEnabled) {
        return false;
    }
    const auto found = g_pack.index.find(hash);
    if (found == g_pack.index.end()) {
        g_pack.misses++;
        return false;
    }

    std::vector<uint8_t> encoded;
    if (!readTexture(found->second, encoded) || encoded.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        std::fprintf(stderr, "[Zelda3D] texpack: failed to read hash %016llX\n",
                     static_cast<unsigned long long>(hash));
        g_pack.misses++;
        return false;
    }

    int channels = 0;
    int probeWidth = 0;
    int probeHeight = 0;
    if (!stbi_info_from_memory(encoded.data(), static_cast<int>(encoded.size()), &probeWidth, &probeHeight, &channels) ||
        probeWidth <= 0 || probeHeight <= 0 || probeWidth > 8192 || probeHeight > 8192 ||
        static_cast<uint64_t>(probeWidth) * static_cast<uint64_t>(probeHeight) > kMaxDecodedPixels) {
        std::fprintf(stderr, "[Zelda3D] texpack: rejected invalid or oversized PNG for hash %016llX\n",
                     static_cast<unsigned long long>(hash));
        g_pack.misses++;
        return false;
    }

    stbi_uc* pixels =
        stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        std::fprintf(stderr, "[Zelda3D] texpack: PNG decode failed for hash %016llX\n",
                     static_cast<unsigned long long>(hash));
        g_pack.misses++;
        return false;
    }
    rgba.assign(pixels, pixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(pixels);

    if (g_pack.ourFlip) {
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        std::vector<uint8_t> temporary(rowBytes);
        for (int y = 0; y < height / 2; y++) {
            uint8_t* top = rgba.data() + static_cast<size_t>(y) * rowBytes;
            uint8_t* bottom = rgba.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
            std::memcpy(temporary.data(), top, rowBytes);
            std::memcpy(top, bottom, rowBytes);
            std::memcpy(bottom, temporary.data(), rowBytes);
        }
    }
    g_pack.hits++;
    return true;
}

TexPackStats TexPackGetStats() {
    std::lock_guard<std::mutex> lock(g_packMutex);
    return TexPackStats{ g_pack.scanned, g_pack.compatible && g_pack.requestedEnabled,
                         static_cast<uint64_t>(g_pack.index.size()), g_pack.hits, g_pack.misses };
}

TexPackDetails TexPackGetDetails() {
    std::lock_guard<std::mutex> lock(g_packMutex);
    return TexPackDetails{
        g_pack.scanned,
        g_pack.requestedEnabled,
        g_pack.compatible,
        g_pack.compatible && g_pack.requestedEnabled,
        g_pack.archive,
        g_pack.hasManifest,
        g_pack.flipPngFiles,
        static_cast<uint64_t>(g_pack.index.size()),
        g_pack.duplicates,
        g_pack.hits,
        g_pack.misses,
        g_pack.generation,
        g_pack.sourcePath.empty() ? g_pack.configuredSource : g_pack.sourcePath,
        g_pack.displayName,
        g_pack.version,
        g_pack.error,
    };
}

uint64_t TexPackGeneration() {
    std::lock_guard<std::mutex> lock(g_packMutex);
    return g_pack.generation;
}

} // namespace Zelda3D
