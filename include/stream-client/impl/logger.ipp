#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace stream_client {
namespace detail {

/// Get/set global logger instance.
inline std::shared_ptr<log_interface> logger_instance(std::shared_ptr<log_interface> new_logger)
{
    // By default use cout logger with trace level
    static std::shared_ptr<log_interface> glob_instance = std::make_shared<cout_logger>();

    if (new_logger) {
        glob_instance = new_logger;
    }
    return glob_instance;
}

} // namespace detail

base_logger::base_logger(log_level level)
    : level_(level)
{
}

void base_logger::set_level(log_level level) noexcept
{
    level_ = level;
}

log_level base_logger::get_level() const noexcept
{
    return level_;
}

func_logger::func_logger(log_level level, stream_client::log_func_type log_func)
    : base_logger(level)
    , log_func_(log_func)
{
}

void func_logger::message(log_level level, const std::string& location, const std::string& message) const
{
    if (log_func_) {
        log_func_(level, location, message);
    }
}

cout_logger::cout_logger(log_level level)
    : base_logger(level)
{
}

void cout_logger::message(log_level level, const std::string& location, const std::string& message) const
{
    static constexpr const char* kLevelPrefixes[] = {
        "TRACE", "DEBUG", "INFO", "WARNING", "ERROR",
    };

    std::stringstream ss;
    const auto now = std::chrono::system_clock::now();
    const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
    std::lock_guard<std::mutex> lock(mutex_);
    ss << std::put_time(std::localtime(&t_c), "%Y-%m-%dT%H:%M:%SZ") << ": " << kLevelPrefixes[static_cast<int>(level)]
       << ": " << location << ": " << message << std::endl;
    std::cout << ss.str();
}

inline void set_logger(std::shared_ptr<log_interface> logger)
{
    detail::logger_instance(std::move(logger));
}

inline void set_logger(log_level level, stream_client::log_func_type log_func)
{
    detail::logger_instance(std::make_shared<func_logger>(level, std::move(log_func)));
}

inline void set_log_level(log_level level)
{
    const auto logger = detail::logger_instance();
    if (logger) {
        logger->set_level(level);
    }
}

inline log_level get_log_level()
{
    const auto logger = detail::logger_instance();
    if (logger) {
        return logger->get_level();
    }
    return log_level::mute;
}

inline void log_message(log_level level, const std::string& location, const std::string& message)
{
    const auto logger = detail::logger_instance();
    if (logger) {
        logger->message(level, location, message);
    }
}

} // namespace stream_client
