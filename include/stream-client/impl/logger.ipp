#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace stream_client {

namespace detail {

inline const log_interface* logger_instance_impl(const log_interface* new_logger = nullptr) {
    static const log_interface* glob_instance = nullptr;
    if (new_logger) {
        glob_instance = new_logger;
    }
    return glob_instance;
}

inline logger_message_func logger_func_impl(logger_message_func msg_func = nullptr) {
    static logger_message_func glob_func = nullptr;
    if (msg_func) {
        glob_func = msg_func;
    }
    return glob_func;
}

static constexpr const char* LEVEL_PREFIX[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
    "TRACE",
};

class std_streams_logger: public log_interface
{
public:
    std_streams_logger(log_level level)
        : level_(level)
    {}

    void message(log_level level, const std::string& location, const std::string& message) const override {
        std::ostream* stream = nullptr;
        if (level > level_) {
            return;
        }
        switch (level) {
            case log_level::error:
            case log_level::warning:
                stream = &std::cerr;
                break;
            case log_level::info:
            case log_level::debug:
            case log_level::trace:
                stream = &std::cout;
                break;
            default:
                return;
        };
        std::stringstream ss;
        const auto now = std::chrono::system_clock::now();
        const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
        ss << ": " << LEVEL_PREFIX[static_cast<int>(level)] << ": " << location << ": " << message << std::endl;

        std::lock_guard<std::mutex> lock(mutex_);
        *stream << std::put_time(std::localtime(&t_c), "%Y-%m-%dT%H:%M:%SZ") << ss.str();
    }

    const log_level level_;
    mutable std::mutex mutex_;
};

} // namespace detail

inline void set_logger_instance(const log_interface& logger) {
    detail::logger_instance_impl(&logger);
}

inline void set_logger_func(logger_message_func msg_func) {
    detail::logger_func_impl(msg_func);
}

inline std::unique_ptr<log_interface> make_std_streams_logger(log_level level) {
    return std::make_unique<detail::std_streams_logger>(level);
}

inline void LOG_LEVEL(log_level level, const std::string& location, const std::string& message) {
    const auto logger_func = detail::logger_func_impl();
    if (logger_func) {
        logger_func(level, location, message);
    }
    const auto* logger_instance = detail::logger_instance_impl();
    if (logger_instance) {
        logger_instance->message(level, location, message);
    }
}

inline void LOG_ERROR(const std::string& location, const std::string& message) {
    LOG_LEVEL(log_level::error, location, message);
}

inline void LOG_WARN(const std::string& location, const std::string& message) {
    LOG_LEVEL(log_level::warning, location, message);
}

inline void LOG_INFO(const std::string& location, const std::string& message) {
    LOG_LEVEL(log_level::info, location, message);
}

inline void LOG_DEBUG(const std::string& location, const std::string& message) {
    LOG_LEVEL(log_level::debug, location, message);
}

inline void LOG_TRACE(const std::string& location, const std::string& message) {
    LOG_LEVEL(log_level::trace, location, message);
}

} // namespace stream_client
