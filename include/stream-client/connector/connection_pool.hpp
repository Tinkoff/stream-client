#pragma once

#include "connector.hpp"
#include "pool_strategy.hpp"

#include <atomic>
#include <list>

namespace stream_client {
namespace connector {

/**
 * Class to maintain filled pool of connected sockets (sockets).
 *
 * This class own instance of @p Connector with which it automatically fills
 * internal std::list of sockets (Connector::stream_type). Internal connector is responsible for DNS-resolution and
 * spawning connected sockets via its' new_session() method. Once stream in pulled from the pool with @p get_session()
 * it will replaced with new one, for reuse of already established sockets you should return them back with
 * return_session().
 *
 * @note Thread-safe. Single instance support concurrent operation.
 *
 * @tparam Connector Type of connector to use to create sockets.
 * @tparam Strategy Type of reconnection strategy. For more info look in pool_strategy.hpp.
 */
template <typename Connector, typename Strategy = greedy_strategy<Connector>>
class base_connection_pool
{
public:
    using connector_type = typename std::remove_reference<Connector>::type;
    using stream_type = typename connector_type::stream_type;
    using protocol_type = typename stream_type::protocol_type;

    using clock_type = typename connector_type::clock_type;
    using time_duration_type = typename connector_type::time_duration_type;
    using time_point_type = typename connector_type::time_point_type;

    /**
     * Parametrized constructor.
     * Constructs pool for desired connector (protocol). Passed @p argn forwarded to @p Connector constructor.
     * This operation starts background thread to fill the pool with opened sockets,
     * therefore subsequent get_session() calls may take longer time compared with the state when pool is full.
     *
     * @tparam ...ArgN Types of argn.
     *
     * @param[in] size Number of connected sockets to maintain in the pool.
     *      Note that real number of established connections my be @p size + 1.
     *      This happens when you pull a stream with get_session() , the pool establishes new one to replace it,
     *      and later you return pulled stream back with return_session().
     * @param[in] idle_timeout sessions which are in the pool for a longer time are replaced with new ones.
     * @param[in] ...argn Arguments to pass to @p Connector constructor.
     */
    template <typename... ArgN>
    base_connection_pool(std::size_t size, time_duration_type idle_timeout, ArgN&&... argn);

    /**
     * Parametrized constructor.
     * Constructs pool for desired connector (protocol). Passed @p argn forwarded to @p Connector constructor.
     * This operation starts background thread to fill the pool with opened sockets,
     * therefore subsequent get_session() calls may take longer time compared with the state when pool is full.
     *
     * @tparam ...ArgN Types of argn.
     *
     * @param[in] size Number of connected sockets to maintain in the pool.
     *      Note that real number of established connections my be @p size + 1.
     *      This happens when you pull a stream with get_session() , the pool establishes new one to replace it,
     *      and later you return pulled stream back with return_session().
     * @param[in] ...argn Arguments to pass to @p Connector constructor.
     */
    template <typename Arg1, typename... ArgN,
              typename std::enable_if<
                  !std::is_convertible<Arg1, typename Connector::time_duration_type>::value>::type* = nullptr>
    base_connection_pool(std::size_t size, Arg1&& arg1, ArgN&&... argn);

    /// Copy constructor is not permitted.
    base_connection_pool(const base_connection_pool<Connector>& other) = delete;
    /// Copy assignment is not permitted.
    base_connection_pool<Connector>& operator=(const base_connection_pool<Connector>& other) = delete;
    /// Move constructor is not permitted.
    base_connection_pool(base_connection_pool<Connector>&& other) = delete;
    /// Move assignment is not permitted.
    base_connection_pool<Connector>& operator=(base_connection_pool<Connector>&& other) = delete;

    /// Destructor.
    virtual ~base_connection_pool();

    /**
     * Pull a session (stream) from the pool.
     * Tries to get stream from the pool until specified deadline is reached.
     *
     * @note Since returned stream was established prior to this call, there is no guarantee it's still
     *      valid to send or receive data. It may be closed on server-side because of inactivity or anything else.
     *      It this case you may retry to get_session() until you get valid one.
     *
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A stream wrapped in std::unique_ptr or nullptr.
     */
    std::unique_ptr<stream_type> get_session(boost::system::error_code& ec, const time_point_type& deadline);

    /**
     * Pull a session (stream) from the pool.
     * Tries to get stream from the pool until specified timeout elapsed.
     *
     * @note Since returned stream was established prior to this call, there is no guarantee it's still
     *      valid to send or receive data. It may be closed on server-side because of inactivity or anything else.
     *      It this case you may retry to get_session() until you get valid one.
     *
     * @param[in] timeout Expiration duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A stream wrapped in std::unique_ptr or nullptr.
     */
    inline std::unique_ptr<stream_type> get_session(boost::system::error_code& ec, const time_duration_type& timeout)
    {
        return get_session(ec, clock_type::now() + timeout);
    }

    /**
     * Pull a session (stream) from the pool.
     * Get stream from the pool. Timeout for this operation is defined by
     * default connection timeout (returned by get_connect_timeout).
     *
     * @note Since returned stream was established prior to this call, there is no guarantee it's still
     *      valid to send or receive data. It may be closed on server-side because of inactivity or anything else.
     *      It this case you may retry to get_session() until you get valid one.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A stream wrapped in std::unique_ptr or nullptr.
     */
    inline std::unique_ptr<stream_type> get_session(boost::system::error_code& ec)
    {
        return get_session(ec, get_connect_timeout());
    }

    /**
     * Pull a session (stream) from the pool.
     * Get stream from the pool. Timeout for this operation is defined by
     * default connection timeout (returned by get_connect_timeout).
     *
     * @note Since returned stream was established prior to this call, there is no guarantee it's still
     *      valid to send or receive data. It may be closed on server-side because of inactivity or anything else.
     *      It this case you may retry to get_session() until you get valid one.
     *
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @returns A stream wrapped in std::unique_ptr. Guaranteed to return valid pointer.
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename Time>
    inline std::unique_ptr<stream_type> get_session(const Time& timeout_or_deadline)
    {
        boost::system::error_code ec;
        auto session = get_session(ec, timeout_or_deadline);
        if (ec) {
            throw boost::system::system_error{ec, "Failed to get a session from the pool connected to " +
                                                      connector_.get_target()};
        }
        if (!session) {
            throw boost::system::system_error{boost::asio::error::operation_aborted,
                                              "Failed to get a session from the pool connected to " +
                                                  connector_.get_target()};
        }
        return session;
    }

    /**
     * Pull a session (stream) from the pool.
     * Get stream from the pool. Timeout for this operation is defined by
     * default connection timeout (returned by get_connect_timeout).
     *
     * @note Since returned stream was established prior to this call, there is no guarantee it's still
     *      valid to send or receive data. It may be closed on server-side because of inactivity or anything else.
     *      It this case you may retry to get_session() until you get valid one.
     *
     * @returns A stream wrapped in std::unique_ptr. Guaranteed to return valid pointer.
     * @throws boost::system::system_error Thrown on failure.
     */
    inline std::unique_ptr<stream_type> get_session()
    {
        return get_session(get_connect_timeout());
    }

    /**
     * Try to pull a session (stream) from the pool.
     * Tries to get stream from the pool until specified deadline is reached.
     *
     * @note Unlike the get_session method, this method doesn't wait to fill up the pool in a timeout.
     *
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A stream wrapped in std::unique_ptr or nullptr.
     */
    std::unique_ptr<stream_type> try_get_session(boost::system::error_code& ec, const time_point_type& deadline);

    /**
     * Try to pull a session (stream) from the pool.
     * Tries to get stream from the pool until specified timeout elapsed.
     *
     * @note Unlike the get_session method, this method doesn't wait to fill up the pool in a timeout.
     *
     * @param[in] timeout Expiration duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A stream wrapped in std::unique_ptr or nullptr.
     */
    inline std::unique_ptr<stream_type> try_get_session(boost::system::error_code& ec,
                                                        const time_duration_type& timeout)
    {
        return try_get_session(ec, clock_type::now() + timeout);
    }

    /**
     * Return the session pulled earlier from the pool.
     *
     * @note Is is better to return sessions after successful usage, otherwise pool will try to refill itself and
     *      if you pulling sessions fast enough you will get new sockets spawned constantly.
     *
     * @remark Return session only if you don't expect incoming data to appear or handle such cases by yourself.
     */
    void return_session(std::unique_ptr<stream_type>&& session);

    /**
     * Check if pool has at least one stream.
     * Waits until @p deadline for pool to become non-empty.
     *
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to boost::asio::error::timed_out if failed to acquire lock over the pool.
     *
     * @returns true - if pool is not empty; false - is it is empty or timeout reached.
     */
    bool is_connected(boost::system::error_code& ec, const time_point_type& deadline) const;

    /**
     * Check if pool has at least one stream.
     * Waits for @p timeout for pool to become non-empty.
     *
     * @param[in] deadline Expiration duration.
     * @param[out] ec Set to boost::asio::error::timed_out if failed to acquire lock over the pool.
     *
     * @returns true - if pool is not empty; false - is it is empty or timeout reached.
     */
    inline bool is_connected(boost::system::error_code& ec, const time_duration_type& timeout) const
    {
        return is_connected(ec, clock_type::now() + timeout);
    }

    /**
     * Check if pool has at least one stream.
     * Waits for pool to become non-empty. Timeout for this operation is defined by
     * default connection timeout (returned by get_connect_timeout).
     *
     * @param[out] ec Set to boost::asio::error::timed_out if failed to acquire lock over the pool.
     *
     * @returns true - if pool is not empty; false - is it is empty or timeout reached.
     */
    inline bool is_connected(boost::system::error_code& ec) const
    {
        return is_connected(ec, clock_type::now() + get_connect_timeout());
    }

    /**
     * Check if pool has at least one stream.
     * Waits for pool to become non-empty. Timeout for this operation is defined by
     * default connection timeout (returned by get_connect_timeout).
     *
     * @returns true - if pool is not empty; false - is it is empty.
     * @throws boost::system::system_error Thrown with boost::asio::error::timed_out in case of timeout.
     */
    inline bool is_connected() const
    {
        boost::system::error_code ec;
        auto empty = is_connected(ec);
        if (ec) {
            throw boost::system::system_error{ec, "Failed to lock the pool connected to " + connector_.get_target()};
        }
        return empty;
    }

    /**
     * Get resolve operations timeout.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& get_resolve_timeout() const
    {
        return connector_.get_resolve_timeout();
    }

    /**
     * Get connect operations default timeout.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& get_connect_timeout() const
    {
        return connector_.get_connect_timeout();
    }

    /**
     * Get I/O timeout for operations on sockets in the pool.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& get_operation_timeout() const
    {
        return connector_.get_operation_timeout();
    }

private:
    /// Background routine used to maintain the pool.
    void watch_pool_routine();

    std::string name_; ///< Name pf the pool.
    Strategy reconnection_; ///< Instance of reconnection strategy used to fill the pool.
    connector_type connector_; ///< Underlying connector used to establish sockets.

    std::size_t pool_max_size_; ///< Number of stream to keep in the @p sesson_pool_.
    time_duration_type idle_timeout_; ///< Idle timeout for the sessions in the @p sesson_pool_.
    std::list<std::pair<time_point_type, std::unique_ptr<stream_type>>>
        sesson_pool_; ///< The list of established sockets.
    mutable std::timed_mutex pool_mutex_; ///< @p sesson_pool_ mutex.
    mutable std::condition_variable_any pool_cv_; ///< @p sesson_pool_ condition variable.

    std::atomic_bool watch_pool_{false}; ///< Flag to stop @p pool_watcher_.
    std::thread pool_watcher_; ///< Thread to run watch_pool_routine() in.

    bool ensure_session(std::unique_lock<std::timed_mutex>& pool_lk, boost::system::error_code& ec,
                        const time_point_type& deadline) const;
};

//! Connections pool with sockets over plain TCP protocol.
using tcp_pool = base_connection_pool<tcp_connector>;
using tcp_conservative_pool = base_connection_pool<tcp_connector, conservative_strategy<tcp_connector>>;
//! Connections pool with sockets over plain UDP protocol.
using udp_pool = base_connection_pool<udp_connector>;
using udp_conservative_pool = base_connection_pool<udp_connector, conservative_strategy<udp_connector>>;

//! Connections pool with sockets over encrypted TCP protocol.
using ssl_pool = base_connection_pool<ssl_connector>;
using ssl_conservative_pool = base_connection_pool<ssl_connector, conservative_strategy<ssl_connector>>;

//! Connections pool with sockets over HTTP protocol.
using http_pool = base_connection_pool<http_connector>;
using http_conservative_pool = base_connection_pool<http_connector, conservative_strategy<http_connector>>;
//! Connections pool with sockets over HTTPS protocol.
using https_pool = base_connection_pool<https_connector>;
using https_conservative_pool = base_connection_pool<https_connector, conservative_strategy<https_connector>>;

} // namespace connector
} // namespace stream_client

#include "impl/connection_pool.ipp"
