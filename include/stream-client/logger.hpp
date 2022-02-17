#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

// TODO: Move to __FILE_NAME__ with gcc 12.1+
#define FILE_NAME (__builtin_strrchr("/" __FILE__, '/') + 1)

/**
 * Internal macro.
 * Used to not evaluate STREAM_LOG_...() arguments if the current level is not enough.
 */
#ifndef STREAM_LOG_CALL
#define STREAM_LOG_CALL(level, ...)                                                                       \
    do {                                                                                                  \
        const auto logger = stream_client::detail::logger_instance();                                     \
        if (logger && logger->get_level() <= level) {                                                     \
            logger->message(level, std::string(FILE_NAME) + ":" + std::to_string(__LINE__), __VA_ARGS__); \
        }                                                                                                 \
    } while (0)
#endif

/**
 * Logs the message if the current log level >= to the ERROR level.
 *
 * @note Doesn't construct the message if the current level is less than the ERROR level
 */
#ifndef STREAM_LOG_ERROR
#define STREAM_LOG_ERROR(...) STREAM_LOG_CALL(stream_client::log_level::error, __VA_ARGS__)
#endif

/**
 * Logs the message if the current log level >= to the WARN level.
 *
 * @note Doesn't construct the message if the current level is less than the WARN level
 */
#ifndef STREAM_LOG_WARN
#define STREAM_LOG_WARN(...) STREAM_LOG_CALL(stream_client::log_level::warning, __VA_ARGS__)
#endif

/**
 * Logs the message if the current log level >= to the INFO level.
 *
 * @note Doesn't construct the message if the current level is less than the INFO level
 */
#ifndef STREAM_LOG_INFO
#define STREAM_LOG_INFO(...) STREAM_LOG_CALL(stream_client::log_level::info, __VA_ARGS__)
#endif

/**
 * Logs the message if the current log level >= to the DEBUG level.
 *
 * @note Doesn't construct the message if the current level is less than the DEBUG level
 */
#ifndef STREAM_LOG_DEBUG
#define STREAM_LOG_DEBUG(...) STREAM_LOG_CALL(stream_client::log_level::debug, __VA_ARGS__)
#endif

/**
 * Logs the message if the current log level >= to the trace level.
 *
 * @note Doesn't construct the message if the current level is less than the trace level
 */
#ifndef STREAM_LOG_TRACE
#define STREAM_LOG_TRACE(...) STREAM_LOG_CALL(stream_client::log_level::trace, __VA_ARGS__)
#endif

namespace stream_client {

// Forward declaration
class log_interface;

namespace detail {

/// Get/set global logger instance.
inline std::shared_ptr<log_interface> logger_instance(std::shared_ptr<log_interface> new_logger = nullptr);

} // namespace detail

/**
 * Base enum of log levels
 */
enum class log_level : int
{
    mute = -1,
    trace = 0,
    debug,
    info,
    warning,
    error,
};

/**
 * Type of the function used to log messages by the library logger.
 *
 * @param[in] level Level of the message.
 * @param[in] location Location of the message. Used for filtering messages from different parts of the library.
 * @param[in] message The body and main reason of message.
 *
 * @note  Thread-safe.
 */
using log_func_type = std::function<void(log_level level, const std::string& location, const std::string& message)>;

/**
 * Logger interface used by the library.
 */
class log_interface
{
public:
    /// Destructor.
    virtual ~log_interface() = default;

    /**
     * Set logger level.
     *
     * @param[in] level Level to setup.
     */
    virtual void set_level(log_level level) noexcept = 0;

    /**
     * Get logger level.
     *
     * @returns Actual logger level.
     */
    virtual log_level get_level() const noexcept = 0;

    /**
     * Log @p message produced from @p location with @p level.
     *
     * @param[in] level Level of the message.
     * @param[in] location Location of the message. Used to filter messages from different parts of the library.
     * @param[in] message The body of the message.
     *
     * @note Thread-safe.
     */
    virtual void message(log_level level, const std::string& location, const std::string& message) const = 0;
};

/// Basic logger with implemented get_level/set_level methods.
class base_logger: public stream_client::log_interface
{
public:
    base_logger(log_level level);

    /// Set logger level.
    virtual void set_level(log_level level) noexcept override;

    /// Get logger level.
    virtual log_level get_level() const noexcept override;

private:
    log_level level_; ///< Logger level.
};

/// Logger calls passed callback function to log messages.
class func_logger: public stream_client::base_logger
{
public:
    func_logger(log_level level, stream_client::log_func_type log_func);

    /// Copy constructor.
    func_logger(const func_logger& other) = default;
    /// Copy assignment.
    func_logger& operator=(const func_logger& other) = default;
    /// Move constructor.
    func_logger(func_logger&& other) = default;
    /// Move assignment.
    func_logger& operator=(func_logger&& other) = default;

    /// Destructor.
    virtual ~func_logger() = default;

    /// Log arbitrary message.
    virtual void message(log_level level, const std::string& location, const std::string& message) const override;

private:
    stream_client::log_func_type log_func_; ///< Log function to call.
};

/// Logger print messages to std::cout with time and level.
class cout_logger: public stream_client::base_logger
{
public:
    /// Construct std::cout logger with @p level.
    cout_logger(log_level level = log_level::trace);

    /// Copy constructor.
    cout_logger(const cout_logger& other) = delete;
    /// Copy assignment.
    cout_logger& operator=(const cout_logger& other) = delete;
    /// Move constructor.
    cout_logger(cout_logger&& other) = delete;
    /// Move assignment.
    cout_logger& operator=(cout_logger&& other) = delete;

    /// Destructor.
    virtual ~cout_logger() = default;

    /// Log arbitrary message.
    virtual void message(log_level level, const std::string& location, const std::string& message) const override;

private:
    mutable std::mutex mutex_; ///< Mutex to sync std::cout calls.
};

/**
 * Set logger for the library.
 *
 * @note All set_logger() variants overwrite each other. You can either set it via callback or an instance.
 *
 * @param[in] logger Logger instance to use. If nullptr logger will print messages to std::cout up to trace level.
 */
inline void set_logger(std::shared_ptr<log_interface> logger);

/**
 * Set log callback for the library.
 *
 * @note All set_logger() variants overwrite each other. You can either set it via callback or an instance.
 *
 * @param[in] level Logger level to use. The level can be changed after with set_log_level().
 * @param[in] log_func Logger function to use.
 */
inline void set_logger(log_level level, stream_client::log_func_type log_func);

/**
 * Set logger level.
 *
 * @param[in] level Level to setup.
 */
inline void set_log_level(log_level level);

/**
 * Get logger level.
 *
 * @returns Actual logger level or log_level::mute if the library logger is not set.
 */
inline log_level get_log_level();

/**
 * Log @p message produced from @p location with @p level.
 *
 * @param[in] level Level of the message.
 * @param[in] location Location of the message.
 * @param[in] message The body of the message.
 *
 * @note Thread-safe.
 */
inline void log_message(log_level level, const std::string& location, const std::string& message);

} // namespace stream_client

#include "impl/logger.ipp"
