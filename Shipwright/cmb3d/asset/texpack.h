// Hi-res custom texture pack lookup (Citra/Azahar-format packs, e.g. Henriko's OoT3D 4K).
// Replaces a CMB texture's decoded RGBA with a same-content hi-res PNG, located by the
// texture's Citra legacy hash (see PicaLegacyHashBytes + CityHash64). The pack is a
// user-provided drop-in (gitignored, ~GB); when absent the feature is simply inactive.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Zelda3D {

// On a hit, fills `rgba` with W*H*4 top-down RGBA8 of the replacement and sets w/h, then
// returns true. The pack is indexed lazily on first call. Returns false (no replacement)
// when no pack is found or the hash is absent.
bool TexPackLookup(uint64_t hash, int& w, int& h, std::vector<uint8_t>& rgba);

// Select a pack source explicitly. `sourcePath` may be either an extracted Citra/Azahar
// texture-pack directory or the original ZIP containing it; ZIPs are indexed and read in place,
// never extracted. Calling this again is also the supported rescan operation. An empty path means
// no installed source. `enabled=false` still validates/indexes the source for the settings UI, but
// lookups remain inactive until it is enabled.
void TexPackConfigure(const std::string& sourcePath, bool enabled);

// Change only the active gate while retaining the already-built index and open ZIP handle. This
// keeps an on/off menu toggle instant even for multi-gigabyte packs. The generation changes when
// the gate changes so model/HUD caches can rebuild against the new source policy.
void TexPackSetEnabled(bool enabled);

// Restore the historical developer behavior (ZELDA3D_TEXPACK, ./textures, ROM-adjacent textures).
// Packaged game code should normally call TexPackConfigure with its app-data selection instead.
void TexPackUseAutoDiscovery();

// Force the configured source to be inspected now rather than on the first texture lookup.
// Returns true when a compatible legacy-hash pack containing at least one mip-0 PNG was found.
bool TexPackScan();

// Whether a pack was found (only meaningful after the first lookup), how many textures
// are indexed, and how many lookups hit / missed it. The parity harness reports these so
// "hi-res is actually in effect" is a measurement rather than an assumption.
struct TexPackStats {
    bool scanned;
    bool active;
    uint64_t indexed;
    uint64_t hits;
    uint64_t misses;
};
TexPackStats TexPackGetStats();

// Full status used by the packaged-game manager and deterministic tests. A pack is compatible when
// its filenames use Citra's legacy 64-bit hash and, when a title-ID directory is present, that ID is
// OoT3D USA (0004000000033500). `active` additionally requires the user's enable switch.
struct TexPackDetails {
    bool scanned;
    bool requestedEnabled;
    bool compatible;
    bool active;
    bool archive;
    bool hasManifest;
    bool flipPngFiles;
    uint64_t indexed;
    uint64_t duplicates;
    uint64_t hits;
    uint64_t misses;
    uint64_t generation;
    std::string sourcePath;
    std::string displayName;
    std::string version;
    std::string error;
};

TexPackDetails TexPackGetDetails();
uint64_t TexPackGeneration();

} // namespace Zelda3D
