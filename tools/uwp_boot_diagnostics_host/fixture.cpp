#include <ship/zelda3d_core.h>
#include <Windows.h>

#include <cstdint>
#include <cstdio>

namespace {
int gMode = 0;
volatile std::uintptr_t gFaultAddress = 1;
}

extern "C" __declspec(dllexport) __declspec(noinline) int FusionFaultRead() {
    return *reinterpret_cast<volatile int*>(gFaultAddress);
}

extern "C" __declspec(dllexport) __declspec(noinline) int FusionFaultWrite() {
    *reinterpret_cast<volatile int*>(gFaultAddress) = 42;
    return 0;
}

extern "C" __declspec(dllexport) __declspec(noinline) int FusionFaultExecute() {
    const int result = reinterpret_cast<int (*)()>(gFaultAddress)();
    std::fputs("execute unexpectedly returned\n", stderr);
    return result;
}

namespace {
__declspec(noinline) int FixtureRecurse(int depth) {
    const int value = depth > 0 ? FixtureRecurse(depth - 1) : FusionFaultRead();
    // Prevent a tail-call so repeated return PCs exercise a real recursive stack.
    std::fprintf(stderr, "recursive frame %d returned\n", depth);
    return value + depth;
}

__declspec(noinline) int FixtureRun(int mode, char**) {
    // Intentionally no fflush: production redirection must make these durable
    // even when the core faults before constructing its own file logger.
    std::fputs("fixture.stdout.shared-crt\n", stdout);
    std::fputs("fixture.stderr.shared-crt\n", stderr);
    const char handleLine[] = "fixture.stderr.win32-handle\n";
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), handleLine, sizeof(handleLine) - 1, &written, nullptr);
    const int result = mode == 1 ? FusionFaultRead() : mode == 2 ? FusionFaultWrite() :
                       mode == 3 ? FusionFaultExecute() : mode == 5 ? FixtureRecurse(4) : 23;
    std::fputs("fixture.runner.returned\n", stderr);
    return result;
}

Zelda3DCore gCore{ ZELDA3D_CORE_ABI, "oot", "diagnostic fake core", FixtureRun };
}

extern "C" __declspec(dllexport) void FusionSetFixtureMode(int mode) {
    // Exercise the Store C++ exception runtime as well as shared UCRT stdio.
    try {
        throw mode;
    } catch (int selected) {
        gMode = selected;
    }
}

extern "C" ZELDA3D_CORE_API const Zelda3DCore* Zelda3D_CoreEntry() {
    std::fputs("fixture.descriptor.entered\n", stderr);
    if (gMode == 4) FusionFaultRead();
    return &gCore;
}
