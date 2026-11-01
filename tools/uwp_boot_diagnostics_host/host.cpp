#include <Windows.h>
#include <ship/zelda3d_core.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const int mode = std::atoi(argv[2]);
    HMODULE driver = LoadLibraryExW(L"diagnostics_driver.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                  LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE fixture = LoadLibraryExW(L"diagnostics_fixture.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                   LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (driver == nullptr || fixture == nullptr) {
        std::fprintf(stderr, "native test DLL load failed: %lu\n", GetLastError());
        return 3;
    }
    const auto run = reinterpret_cast<int (*)(const char*, Zelda3DCoreEntryFn, int)>(
        GetProcAddress(driver, "FusionRunDiagnosticTest"));
    const auto entry = reinterpret_cast<Zelda3DCoreEntryFn>(GetProcAddress(fixture, ZELDA3D_CORE_ENTRY_SYMBOL));
    const auto setMode = reinterpret_cast<void (*)(int)>(GetProcAddress(fixture, "FusionSetFixtureMode"));
    const auto read = GetProcAddress(fixture, "FusionFaultRead");
    const auto write = GetProcAddress(fixture, "FusionFaultWrite");
    if (run == nullptr || entry == nullptr || setMode == nullptr || read == nullptr || write == nullptr) return 4;

    FILE* evidence = nullptr;
    const std::string evidencePath = std::string(argv[1]) + "/addresses.json";
    if (fopen_s(&evidence, evidencePath.c_str(), "wb") != 0 || evidence == nullptr) return 5;
    const auto base = reinterpret_cast<std::uintptr_t>(fixture);
    std::fprintf(evidence, "{\"fixture_base\":%llu,\"read_rva\":%llu,\"write_rva\":%llu}\n",
                 static_cast<unsigned long long>(base),
                 static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(read) - base),
                 static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(write) - base));
    std::fclose(evidence);
    setMode(mode);
    // Emulate WinRT activation without a console. The Store diagnostic DLL must
    // attach valid stdio handles before the independently loaded core writes.
    SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
    SetStdHandle(STD_ERROR_HANDLE, nullptr);
    return run(argv[1], entry, mode);
}
