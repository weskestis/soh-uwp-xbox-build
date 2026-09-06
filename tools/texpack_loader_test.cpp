#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "asset/texpack.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kHash = 0x0123456789ABCDEFULL;

[[noreturn]] void Fail(const char* message) {
    std::fprintf(stderr, "texpack_loader_test: FAIL: %s\n", message);
    std::exit(1);
}

void Check(bool condition, const char* message) {
    if (!condition) {
        Fail(message);
    }
}

bool Contains(const std::string& value, const char* needle) {
    return value.find(needle) != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        Fail("expected valid-dir valid-zip new-hash-dir foreign-dir mip1-dir invalid-png-dir");
    }

    Zelda3D::TexPackConfigure(argv[1], false);
    Check(Zelda3D::TexPackScan(), "valid extracted pack was rejected");
    Zelda3D::TexPackDetails details = Zelda3D::TexPackGetDetails();
    Check(details.compatible && !details.active && !details.archive, "disabled directory status is wrong");
    Check(details.indexed == 2, "directory should index current and legacy filename forms");
    Check(details.displayName == "Fixture Folder" && details.version == "v1", "directory manifest was not read");

    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    Check(!Zelda3D::TexPackLookup(kHash, width, height, rgba), "disabled pack served a texture");
    const uint64_t disabledGeneration = Zelda3D::TexPackGeneration();
    Zelda3D::TexPackSetEnabled(true);
    Check(Zelda3D::TexPackGeneration() == disabledGeneration + 1, "enable did not advance generation");
    Zelda3D::TexPackSetEnabled(true);
    Check(Zelda3D::TexPackGeneration() == disabledGeneration + 1, "no-op enable advanced generation");
    Check(Zelda3D::TexPackLookup(kHash, width, height, rgba), "enabled directory lookup missed");
    Check(width == 2 && height == 2 && rgba.size() == 16, "directory PNG dimensions are wrong");
    Check(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 255, "flip_png_files=false did not flip rows");

    Zelda3D::TexPackConfigure(argv[2], true);
    Check(Zelda3D::TexPackScan(), "valid ZIP pack was rejected");
    details = Zelda3D::TexPackGetDetails();
    Check(details.active && details.archive && details.hasManifest, "ZIP status is wrong");
    Check(details.displayName == "Fixture ZIP" && details.version == "v2", "ZIP manifest was not read");
    rgba.clear();
    Check(Zelda3D::TexPackLookup(kHash, width, height, rgba), "ZIP lookup missed");
    Check(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0, "flip_png_files=true unexpectedly flipped rows");

    Zelda3D::TexPackConfigure(argv[3], true);
    Check(!Zelda3D::TexPackScan(), "use_new_hash=true pack was accepted");
    Check(Contains(Zelda3D::TexPackGetDetails().error, "use_new_hash=true"), "new-hash rejection is not explicit");

    Zelda3D::TexPackConfigure(argv[4], true);
    Check(!Zelda3D::TexPackScan(), "foreign-title pack was accepted");
    Check(Contains(Zelda3D::TexPackGetDetails().error, "different 3DS title"), "title rejection is not explicit");

    Zelda3D::TexPackConfigure(argv[5], true);
    Check(!Zelda3D::TexPackScan(), "mip1-only pack was accepted");
    Check(Contains(Zelda3D::TexPackGetDetails().error, "mip-0"), "mip rejection is not explicit");

    Zelda3D::TexPackConfigure(argv[6], true);
    Check(Zelda3D::TexPackScan(), "invalid-PNG fixture should pass index validation");
    rgba.clear();
    Check(!Zelda3D::TexPackLookup(kHash, width, height, rgba), "invalid PNG was decoded");

    const Zelda3D::TexPackStats stats = Zelda3D::TexPackGetStats();
    Check(stats.scanned && stats.active && stats.indexed == 1 && stats.misses == 1,
          "invalid-PNG lookup statistics are wrong");
    std::puts("texpack_loader_test: PASS");
    return 0;
}
