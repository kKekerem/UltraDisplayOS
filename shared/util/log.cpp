#include "log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <atomic>

namespace ud {

static std::atomic<bool> g_logging_suspended{false};

class SuspendedFilterSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit SuspendedFilterSink(spdlog::sink_ptr wrapped_sink) 
        : wrapped_sink_(std::move(wrapped_sink)) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Critical log messages still pass through even if suspended
        if (!g_logging_suspended.load(std::memory_order_relaxed) || msg.level >= spdlog::level::err) {
            wrapped_sink_->log(msg);
        }
    }
    
    void flush_() override {
        wrapped_sink_->flush();
    }

private:
    spdlog::sink_ptr wrapped_sink_;
};


void init_logging(bool is_server, bool enable_debug) {
    spdlog::init_thread_pool(8192, 1); // 8k queue, 1 background thread

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%dT%H:%M:%S.%f] [%^%l%$] [%s:%#] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        is_server ? "ultradisplay_server.log" : "ultradisplay_client.log", true);
    file_sink->set_pattern("[%Y-%m-%dT%H:%M:%S.%f] [%l] [%s:%#] %v");

    auto filtered_console = std::make_shared<SuspendedFilterSink>(console_sink);
    auto filtered_file = std::make_shared<SuspendedFilterSink>(file_sink);

    std::vector<spdlog::sink_ptr> sinks {filtered_console, filtered_file};

    auto logger = std::make_shared<spdlog::async_logger>(
        "ud", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

    logger->set_level(enable_debug ? spdlog::level::debug : spdlog::level::info);
    
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(2));
}

void suspend_logging(bool suspend) {
    g_logging_suspended.store(suspend, std::memory_order_release);
}

} // namespace ud
