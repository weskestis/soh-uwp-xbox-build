#pragma once

// The fixture controls only the output directory. Production diagnostic and
// bootstrap sources are compiled unchanged against this minimal SDL boundary.
extern "C" char* SDL_GetPrefPath(const char* organization, const char* application);
extern "C" void SDL_free(void* memory);
