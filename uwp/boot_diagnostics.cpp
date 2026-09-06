#include "boot_diagnostics.h"

#include "core_bootstrap.h"

#include <Windows.h>
#include <SDL2/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifndef ZELDA3D_UWP_PACKAGE_VERSION
#define ZELDA3D_UWP_PACKAGE_VERSION "unknown"
#endif

namespace {

constexpr char kLogName[] = "uwp-boot.log";
char gLogPath[4096] = {};

void DebugOutput(const char* line) {
    OutputDebugStringA(line);
    OutputDebugStringA("\r\n");
}

void WriteLine(const char* line, const char* mode = "ab") {
    DebugOutput(line);
    if (gLogPath[0] == '\0') {
        return;
    }

    if (FILE* file = std::fopen(gLogPath, mode)) {
        std::fputs(line, file);
        std::fputs("\r\n", file);
        std::fflush(file);
        std::fclose(file);
    }
}

void WriteFormat(const char* format, ...) {
    char line[1024] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    WriteLine(line);
}

LONG CoreExceptionFilter(EXCEPTION_POINTERS* exception) {
    const DWORD code = exception != nullptr && exception->ExceptionRecord != nullptr
                           ? exception->ExceptionRecord->ExceptionCode
                           : 0;
    const void* address = exception != nullptr && exception->ExceptionRecord != nullptr
                              ? exception->ExceptionRecord->ExceptionAddress
                              : nullptr;
    WriteFormat("stage=core.exception code=0x%08lX address=%p", code, address);
    return EXCEPTION_EXECUTE_HANDLER;
}

void ProbePackagedRuntime() {
    struct ModuleProbe {
        const wchar_t* filename;
        const char* label;
    };
    static const ModuleProbe probes[] = {
        { L"SDL2.dll", "SDL2.dll" },
        { L"libuwp.dll", "libuwp.dll" },
        { L"z-1.dll", "z-1.dll" },
        { L"dxil.dll", "dxil.dll" },
        { L"glfw3.dll", "glfw3.dll" },
        { L"libgallium_wgl.dll", "libgallium_wgl.dll" },
        { L"opengl32.dll", "opengl32.dll" },
    };

    Zelda3DUwp_BootLog("runtime-probe.begin");
    for (const ModuleProbe& probe : probes) {
        if (GetModuleHandleW(probe.filename) != nullptr) {
            WriteFormat("runtime.module=%s state=already-loaded", probe.label);
            continue;
        }

        HMODULE module = LoadPackagedLibrary(probe.filename, 0);
        if (module == nullptr) {
            WriteFormat("runtime.module=%s state=failed win32=%lu", probe.label, GetLastError());
            continue;
        }
        WriteFormat("runtime.module=%s state=load-ok", probe.label);
        FreeLibrary(module);
    }
    Zelda3DUwp_BootLog("runtime-probe.end");
}

// Keep this function free of C++ objects that require unwinding: MSVC rejects
// __try in a function with destructors (C2712).
int RunCoreProtected(Zelda3DCoreEntryFn entry, int argc, char** argv) {
    __try {
        Zelda3DUwp_BootLog("core.run.enter");
        return Zelda3DUwp_RunCore(entry, argc, argv);
    } __except (CoreExceptionFilter(GetExceptionInformation())) {
        return ZELDA3D_UWP_UNHANDLED_EXCEPTION;
    }
}

} // namespace

void Zelda3DUwp_BootLogStart() {
    gLogPath[0] = '\0';
    char* directory = SDL_GetPrefPath(nullptr, "soh");
    if (directory == nullptr) {
        WriteLine("stage=log.path.failed");
        return;
    }

    const size_t directoryLength = std::strlen(directory);
    const bool needsSeparator = directoryLength > 0 && directory[directoryLength - 1] != '/' &&
                                directory[directoryLength - 1] != '\\';
    if (directoryLength + (needsSeparator ? 1 : 0) + sizeof(kLogName) > sizeof(gLogPath)) {
        SDL_free(directory);
        WriteLine("stage=log.path.too-long");
        return;
    }

    std::memcpy(gLogPath, directory, directoryLength);
    size_t filenameOffset = directoryLength;
    if (needsSeparator) {
        gLogPath[filenameOffset++] = '/';
    }
    std::memcpy(gLogPath + filenameOffset, kLogName, sizeof(kLogName));
    SDL_free(directory);

    WriteLine("SOH Cursor FPS V3 + OoT3D Xbox boot report", "wb");
    WriteFormat("package.version=%s", ZELDA3D_UWP_PACKAGE_VERSION);
    Zelda3DUwp_BootLog("callback.enter");
}

void Zelda3DUwp_BootLog(const char* stage) {
    WriteFormat("stage=%s", stage != nullptr ? stage : "(null)");
}

int Zelda3DUwp_RunPackagedCore(int argc, char** argv) {
    Zelda3DUwp_BootLog("core.load.begin");
    HMODULE coreModule = LoadPackagedLibrary(L"soh_core.dll", 0);
    if (coreModule == nullptr) {
        WriteFormat("stage=core.load.failed win32=%lu", GetLastError());
        ProbePackagedRuntime();
        return ZELDA3D_UWP_CORE_LOAD_FAILED;
    }
    Zelda3DUwp_BootLog("core.load.ok");

    FARPROC symbol = GetProcAddress(coreModule, ZELDA3D_CORE_ENTRY_SYMBOL);
    if (symbol == nullptr) {
        WriteFormat("stage=core.symbol.failed name=%s win32=%lu", ZELDA3D_CORE_ENTRY_SYMBOL,
                    GetLastError());
        FreeLibrary(coreModule);
        return ZELDA3D_UWP_CORE_SYMBOL_FAILED;
    }
    Zelda3DUwp_BootLog("core.symbol.ok");

    const auto entry = reinterpret_cast<Zelda3DCoreEntryFn>(symbol);
    const int result = RunCoreProtected(entry, argc, argv);
    WriteFormat("stage=core.run.return result=%d", result);
    return result;
}
