#include <ship/debug/ReleaseLogger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <cstdlib>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const auto caller = std::this_thread::get_id();
    bool deliveredOnCaller = false;
    auto probe = std::make_shared<spdlog::sinks::callback_sink_mt>(
        [&](const spdlog::details::log_msg&) {
            if (std::this_thread::get_id() == caller) {
                deliveredOnCaller = true;
            }
        });
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(argv[1], true);
    std::shared_ptr<spdlog::details::thread_pool> pool;
    auto logger = Ship::CreateReleaseLogger("boot-test", {probe, file}, pool);
    logger->error("[2ship] cannot boot: missing archive (immediate-exit regression test)");
    logger->flush();
    // No destructors, spdlog shutdown, joining, or grace period can rescue a
    // queued message. The parent must find the complete line after exit 23.
    std::_Exit(deliveredOnCaller ? 23 : 3);
}
