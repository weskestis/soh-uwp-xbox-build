#include "error_dialog.h"

#if defined(ZELDA3D_UWP)
#include <SDL.h>
#else
#include "extractor/Extract.h"
#endif

extern "C" void Messagebox_ShowErrorBox(char* title, char* body) {
#if defined(ZELDA3D_UWP)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title != nullptr ? title : "SOH error",
                             body != nullptr ? body : "Unknown error", nullptr);
#else
    Extractor::ShowErrorBox(title, body);
#endif
}
