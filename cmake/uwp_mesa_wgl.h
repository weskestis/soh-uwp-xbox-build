#pragma once

// These two entry points are supplied by the pinned app-local Mesa DLL.
// They are not calls to the desktop system WGL implementation. The import
// library is generated from that exact DLL and audited against its exports.
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) PROC WINAPI wglGetProcAddress(LPCSTR name);
__declspec(dllimport) HDC WINAPI wglGetCurrentDC(void);
#ifdef __cplusplus
}
#endif
