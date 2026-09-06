#include "repl_fps.h"

#include <chrono>

namespace {

using FpsClock = std::chrono::steady_clock;

struct FpsState {
    FpsClock::time_point samples[128] = {};
    int head = 0;
    int count = 0;
};

FpsState sFps;

} // namespace

namespace Zelda3D::Repl {

void TickFps() {
    sFps.samples[sFps.head] = FpsClock::now();
    sFps.head = (sFps.head + 1) & 127;
    if (sFps.count < 128) {
        ++sFps.count;
    }
}

void ResetFps() {
    sFps = {};
}

} // namespace Zelda3D::Repl

extern "C" double Zelda3D_ReplLogicFpsWindow(void) {
    if (sFps.count < 2) {
        return 0.0;
    }
    const FpsClock::time_point& newest = sFps.samples[(sFps.head + 127) & 127];
    const FpsClock::time_point& oldest = sFps.samples[(sFps.head - sFps.count + 128) & 127];
    return std::chrono::duration<double>(newest - oldest).count();
}

extern "C" int Zelda3D_ReplLogicFpsSamples(void) {
    return sFps.count;
}

extern "C" double Zelda3D_ReplLogicFps(void) {
    const double window = Zelda3D_ReplLogicFpsWindow();
    return window > 0.0 ? (sFps.count - 1) / window : 0.0;
}
