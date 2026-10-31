#pragma once

#include <memory>
#include <string>
#include <vector>
#include <spdlog/async.h>
#include <spdlog/logger.h>

namespace Ship {

// A UWP boot failure can return straight to process shutdown. Queuing an async
// flush is insufficient there, even if the engine retains the pool's owner.
// Keep desktop Release's async behavior and ownership unchanged.
inline std::shared_ptr<spdlog::logger> CreateReleaseLogger(
    const std::string& name, const std::vector<spdlog::sink_ptr>& sinks,
    std::shared_ptr<spdlog::details::thread_pool>& pool) {
#ifdef ZELDA3D_UWP
    pool.reset();
    return std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
#else
    pool = std::make_shared<spdlog::details::thread_pool>(8192, 1);
    return std::make_shared<spdlog::async_logger>(name, sinks.begin(), sinks.end(), pool,
                                                 spdlog::async_overflow_policy::block);
#endif
}

} // namespace Ship
