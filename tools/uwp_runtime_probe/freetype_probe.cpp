#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_CONFIG_OPTIONS_H

#ifdef FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES
#error Store font initialization must not use the process CRT environment
#endif

// Keep real initialization/property code in the native link. No FreeType or
// C++ objects cross this boundary; executing this probe is a separate test.
extern "C" __declspec(dllexport) int FusionUwpFreeTypeProbe() noexcept {
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return 1;
    }
    FT_UInt version = 40;
    FT_UInt actual = 0;
    const auto setResult = FT_Property_Set(library, "truetype", "interpreter-version", &version);
    const auto getResult = FT_Property_Get(library, "truetype", "interpreter-version", &actual);
    const auto doneResult = FT_Done_FreeType(library);
    return setResult || getResult || doneResult || actual != version ? 2 : 0;
}
