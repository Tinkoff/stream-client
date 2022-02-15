#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace stream_client {

namespace logging {

inline const log_interface* get_logger(const log_interface* new_logger = nullptr) {
    static const log_interface* glob = nullptr;
    if (new_logger) {
        glob = new_logger;
    }
    return glob;
}

inline void LOG_ERROR(const std::string& target, const std::string& message, const std::runtime_error& error) {
    const log_interface* logger = get_logger();
    if (logger) {
        logger->message(log_level::error, target, message, &error);
    }
}

inline void LOG_WARN(const std::string& target, const std::string& message) {
    const log_interface* logger = get_logger();
    if (logger) {
        logger->message(log_level::warning, target, message);
    }
}

inline void LOG_INFO(const std::string& target, const std::string& message) {
    const log_interface* logger = get_logger();
    if (logger) {
        logger->message(log_level::info, target, message);
    }
}

class std_streams_logger: public log_interface
{
public:
    std_streams_logger()
        : level_prefix_{
            "ERROR",
            "WARNING",
            "INFO",
        }
    {
    }

    void message(log_level level, const std::string& target, const std::string& message, const std::runtime_error* error = nullptr) const override {
        std::ostream* stream = nullptr;
        switch (level) {
            case log_level::error:
            case log_level::warning:
                stream = &std::cerr;
                break;
            case log_level::info:
                stream = &std::cout;
                break;
            default:
                return;
        };
        std::stringstream ss;
        const auto now = std::chrono::system_clock::now();
        const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
        ss << std::put_time(std::localtime(&t_c), "%Y-%m-%dT%H:%M:%SZ") << ": " << level_prefix_[static_cast<int>(level)] << ": " << target << ": " << message;
        if (error) {
            ss << ": " << error->what();
        }
        ss << std::endl;

        std::lock_guard<std::mutex> lock(mutex_);
        *stream << ss.str();
    }

    std::string level_prefix_[3];
    mutable std::mutex mutex_;
};

} // namespace logging

inline void set_logger(const log_interface& logger) {
    logging::get_logger(&logger);
}

std::unique_ptr<log_interface> make_std_streams_logger() {
    return std::make_unique<logging::std_streams_logger>();
}

} // namespace stream_client
