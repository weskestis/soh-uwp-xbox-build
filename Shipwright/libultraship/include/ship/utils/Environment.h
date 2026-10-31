#pragma once

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

// Desktop environment switches are optional diagnostics/config overrides.
// UWP has no supported CRT environment API; use each caller's unset default.
static inline char* ShipGetEnvironmentVariable(const char* name) {
#ifdef ZELDA3D_UWP
    (void)name;
    return NULL;
#else
    return getenv(name);
#endif
}
