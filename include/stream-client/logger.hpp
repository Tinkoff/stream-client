#pragma once

#include <string>

namespace stream_client {

/**
 * Base enum of log log levels
 */
enum class log_level {
    error = 0,
    warning,
    info,
};

/**
 * Base class for log events from internals of library
 */
class log_interface {
public:
    virtual ~log_interface() {}

    /**
     * Message from library for application.
     *
     * @param[in] level Level of message, application may use correct it according the target if necessary.
     * @param[in] target Target of message is used for filter messages from different parts of library.
     * @param[in] message The body and main reason of message.
     * @param[in] error The error instance if exists.
     *
     * @note It can be from multiple threads, you should worry about sync and not lock this calls.
     */
    virtual void message(log_level level, const std::string& target, const std::string& message, const std::runtime_error* error = nullptr) const = 0;
};

/**
 * Set logger instace for library
 *
 * @param[in] logger Logger interface for usage.
 *
 * @note You should do this once when starting the application and the lifetime of the variable till exit.
 */
void set_logger(const log_interface& logger);

/**
 * Make simple logger of std streams
 */
std::unique_ptr<log_interface> make_std_streams_logger();

} // namespace stream_client

#include "detail/logger.ipp"
