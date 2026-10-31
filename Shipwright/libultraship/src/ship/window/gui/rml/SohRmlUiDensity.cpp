#include "SohRmlUi.h"
#include "ship/utils/Environment.h"
#include "ship/utils/SDLCompat.h"
#include <RmlUi/Core/Context.h>

namespace Ship {

void SohRmlUi::ApplyDensityRatio() {
    if (!mContext || !mSdlWindow) {
        return;
    }
    // SDL3 reports the display content scale (1.0 on a 96-dpi monitor, 1.5/2.0 on HiDPI / fractional
    // scaling). RmlUi multiplies every `dp` unit by this ratio, so the sheet — authored in dp — renders
    // at a consistent physical size across displays. A debug override lets the headless harness verify
    // the scaling math (the Xvfb display always reports 1.0).
#ifdef ZELDA3D_USE_SDL2
    float scale = 1.0f;
#else
    float scale = SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(mSdlWindow));
#endif
    if (const char* e = ShipGetEnvironmentVariable("ZELDA3D_RML_DPI")) {
        float v = (float)atof(e);
        if (v > 0.0f) {
            scale = v;
        }
    }
    if (!(scale > 0.0f)) {
        scale = 1.0f;
    }
    if (scale != mDpRatio) {
        mDpRatio = scale;
        mContext->SetDensityIndependentPixelRatio(scale);
    }
}

} // namespace Ship
