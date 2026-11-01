#include "startup_storage.h"

#include <Windows.h>
#include <cstdio>
#include <string>

namespace {
bool Failed(DWORD error, void (*trace)(const char*)) {
    char message[80] = {};
    std::snprintf(message, sizeof(message), "storage.logs.failed win32=%lu", error);
    if (trace != nullptr) trace(message);
    return false;
}
}

bool Zelda3DUwp_PrepareLogDirectory(const char* directory, void (*trace)(const char*)) {
    if (directory == nullptr || directory[0] == '\0') return Failed(ERROR_PATH_NOT_FOUND, trace);
    wchar_t wideDirectory[4096] = {};
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, directory, -1, wideDirectory,
                            static_cast<int>(sizeof(wideDirectory) / sizeof(wideDirectory[0]))) == 0) {
        return Failed(GetLastError(), trace);
    }
    std::wstring path(wideDirectory);
    if (path.back() != L'/' && path.back() != L'\\') path += L'\\';
    path += L"logs";
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) return Failed(error, trace);
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) return Failed(GetLastError(), trace);
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return Failed(ERROR_DIRECTORY, trace);
    }
    if (trace != nullptr) trace("storage.logs.ready");
    return true;
}
