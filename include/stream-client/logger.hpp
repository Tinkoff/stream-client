#pragma once

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace stream_client {

/**
 * Base enum of log log levels
 */
enum class log_level
{
    error = 0,
    warning,
    info,
    debug,
    trace,
};

/**
 * Message logger function from library for application.
 *
 * @param[in] level Level of message, application may use correct it according the location if necessary.
 * @param[in] location Target of message is used for filter messages from different parts of library.
 * @param[in] message The body and main reason of message.
 *
 * @note It can be from multiple threads, you should worry about sync and not lock this calls.
 */
typedef void (*logger_message_func)(log_level level, const std::string& location, const std::string& message);

/**
 * Base class for log events from internals of library
 */
class log_interface
{
public:
    virtual ~log_interface() {}

    /**
     * See logger_message_func description above
     *
     * @note It can be from multiple threads, you should worry about sync and not lock this calls.
     */
    virtual void message(log_level level, const std::string& location, const std::string& message) const = 0;
};

/**
 * Set logger instance for library
 *
 * @param[in] logger Logger interface for usage.
 *
 * @note You should do this once when starting the application and the lifetime of the variable till exit.
 */
inline void set_logger_instance(const log_interface& logger);

/**
 * Set logger instace for library
 *
 * @param[in] logger Logger interface for usage.
 *
 * @note You should do this once when starting the application and the lifetime of the variable till exit.
 */
inline void set_logger_func(logger_message_func msg_func);

/**
 * Make a simple logger using std streams
 */
inline std::unique_ptr<log_interface> make_std_streams_logger(log_level level = log_level::info);

} // namespace stream_client

#include "impl/logger.ipp"
