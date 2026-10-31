#include <winapifamily.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

#if !defined(_DLL) || WINAPI_FAMILY != WINAPI_FAMILY_APP
#error The probe requires a dynamic CRT and UWP API declarations
#endif

// Only C values cross the boundary. Exercise the STL/CRT features the cores
// need, while preserving exceptions, strings, streams and threads in this DLL.
extern "C" __declspec(dllexport) int FusionUwpRuntimeProbe(const wchar_t* output) noexcept {
    if (output == nullptr) {
        return 2;
    }
    try {
        const std::filesystem::path path(output);
        if (!path.is_absolute()) {
            return 3;
        }
        std::string message;
        std::thread worker([&message] { message = "Fusion runtime probe"; });
        worker.join();
        try {
            throw std::runtime_error(message);
        } catch (const std::exception& error) {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << error.what() << '\n';
            file.flush();
            if (!file) {
                return 4;
            }
        }
        return std::filesystem::file_size(path) > 0 ? 0 : 5;
    } catch (...) {
        return 1;
    }
}
