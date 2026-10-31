#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace MmArchiveBoot {

struct Version {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
};

// portVersion is one endianness byte followed by three uint16 fields. Check
// the whole record before reading any field; truncated data is never version 0.
inline std::optional<Version> DecodeVersion(const uint8_t* bytes, size_t size) {
    if (bytes == nullptr || size < 7 || bytes[0] > 1) {
        return std::nullopt;
    }
    const auto read = [bytes](size_t offset) -> uint16_t {
        return bytes[0] == 1 ? (uint16_t(bytes[offset]) << 8) | bytes[offset + 1]
                             : (uint16_t(bytes[offset + 1]) << 8) | bytes[offset];
    };
    return Version{ read(1), read(3), read(5) };
}

inline bool PortCompatible(const std::optional<Version>& version, Version current) {
    return version && version->major == current.major && version->minor == current.minor &&
           version->patch == current.patch;
}

inline bool RomCompatible(const std::optional<Version>& version, Version current) {
    return version && version->major == current.major;
}

} // namespace MmArchiveBoot
