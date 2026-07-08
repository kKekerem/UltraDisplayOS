#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace ud {

void init_logging(bool is_server, bool enable_debug = false);
void suspend_logging(bool suspend); // Appliance mode feature to avoid IO

} // namespace ud

#define UD_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define UD_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define UD_LOG_INFO(...)  SPDLOG_INFO(__VA_ARGS__)
#define UD_LOG_WARN(...)  SPDLOG_WARN(__VA_ARGS__)
#define UD_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define UD_LOG_FATAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
