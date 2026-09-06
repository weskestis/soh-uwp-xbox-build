#include "frame_timing.h"

#include "soh/OTRGlobals.h"

#include <chrono>
#include <cmath>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

using PresentClock = std::chrono::steady_clock;

PresentClock::time_point sPresentRing[256];
int sPresentHead = 0;
int sPresentCount = 0;

} // namespace

void Zelda3D_RecordPresentedFrame() {
    sPresentRing[sPresentHead] = PresentClock::now();
    sPresentHead = (sPresentHead + 1) & 255;
    if (sPresentCount < 256) {
        ++sPresentCount;
    }
}

#ifdef _WIN32
extern "C" uint64_t GetFrequency(void) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

extern "C" uint64_t GetPerfCounter(void) {
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}
#else
extern "C" uint64_t GetFrequency(void) {
    return 1000;
}

extern "C" uint64_t GetPerfCounter(void) {
    const auto monotonicTime = PresentClock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(monotonicTime).count());
}
#endif

extern "C" uint64_t GetUnixTimestamp(void) {
    const auto sinceEpoch = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch).count());
}

extern "C" double Zelda3D_PresentFps(void) {
    if (sPresentCount < 2) {
        return 0.0;
    }

    const PresentClock::time_point& newest = sPresentRing[(sPresentHead + 255) & 255];
    const PresentClock::time_point& oldest = sPresentRing[(sPresentHead - sPresentCount + 256) & 255];
    const double windowSeconds = std::chrono::duration<double>(newest - oldest).count();
    return windowSeconds > 0.0 ? (sPresentCount - 1) / windowSeconds : 0.0;
}

extern "C" uint32_t OTRGlobals_GetInterpolationFPS(void) {
    return OTRGlobals::Instance->GetInterpolationFPS();
}

extern "C" uint32_t Ship_GetInterpolationFrameCount(void) {
    return static_cast<uint32_t>(std::ceil(static_cast<float>(OTRGlobals::Instance->GetInterpolationFPS()) / 20.0f));
}
