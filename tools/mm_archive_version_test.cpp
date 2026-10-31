#include "host/archive_version.h"

#include <cassert>
#include <cstdio>

int main() {
    using namespace MmArchiveBoot;
    const Version expected{ 0x1234, 0x5678, 0x9abc };
    const uint8_t little[]{ 0, 0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a };
    const uint8_t big[]{ 1, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc };
    for (const auto* record : { little, big }) {
        const auto version = DecodeVersion(record, 7);
        assert(PortCompatible(version, expected));
        assert(RomCompatible(version, expected));
        for (size_t length = 0; length < 7; ++length) {
            assert(!DecodeVersion(record, length));
        }
    }
    assert(!DecodeVersion(nullptr, 7));
    const uint8_t invalidEndian[]{ 2, 0, 0, 0, 0, 0, 0 };
    assert(!DecodeVersion(invalidEndian, sizeof(invalidEndian)));
    assert(!PortCompatible(std::nullopt, expected));
    assert(!RomCompatible(std::nullopt, expected));

    const auto version = DecodeVersion(little, sizeof(little));
    assert(!PortCompatible(version, { 0, expected.minor, expected.patch }));
    assert(!PortCompatible(version, { expected.major, 0, expected.patch }));
    assert(!PortCompatible(version, { expected.major, expected.minor, 0 }));
    assert(!RomCompatible(version, { 0, expected.minor, expected.patch }));
    assert(RomCompatible(version, { expected.major, 0, 0 }));

    // A readable version 0 is distinct from a missing/truncated record.
    const uint8_t zero[]{ 0, 0, 0, 0, 0, 0, 0 };
    assert(PortCompatible(DecodeVersion(zero, sizeof(zero)), { 0, 0, 0 }));
    std::puts("MM archive version policy: endian, truncation, and compatibility checks PASS");
}
