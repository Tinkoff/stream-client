#pragma once

#include "stream-client/resolver/resolver.hpp"
#include "stream-client/stream/http_socket.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace stream_client {
namespace connector {

/**
 * Connector class to obtain new streams (sessions) to desired remote endpoint.
 *
 * This class incorporates stream_client::resolver::base_resolver to provide DNS updates.
 * Address to connect will be chosen randomly if DNS resolved into multiple records.
 *
 * @note Thread-safe. Single instance support concurrent operation.
 *
 * @tparam Stream Type of stream to create upon new_session()
 */
template <typename Stream>
class base_connector
{
public:
    using stream_type = typename std::remove_reference<Stream>::type;
    using protocol_type = typename stream_type::protocol_type;
    using endpoint_type = typename stream_type::endpoint_type;
    using endpoint_container_type = std::vector<endpoint_type>;

    using resolver_type = ::stream_client::resolver::base_resolver<protocol_type>;
    using resolve_flags_type = typename resolver_type::resolve_flags_type;
    using resolver_endpoint_iterator_type = typename resolver_type::iterator_type;

    using clock_type = typename stream_type::clock_type;
    using time_duration_type = typename stream_type::time_duration_type;
    using time_point_type = typename stream_type::time_point_type;

    /**
     * Parametrized constructor.
     * Creates connector with desired settings.
     * This operation starts background resolving thread to obtain list of endpoints.
     * Created connector does not establish any connections by itself, to do it use new_session().
     *
     * @param[in] host Remote hostname.
     * @param[in] port Remote port.
     * @param[in] resolve_timeout DNS resolve timeout, used by internal resolve_routine().
     * @param[in] connect_timeout Default timeout for connecting operation, used by new_session().
     * @param[in] operation_timeout Timeout for I/O operations on established sessions.
     * @param[in] ip_family Family of IP protocol to resolve (ipv4, ipv6 or any).
     * @param[in] resolve_flags A set of flags that determine how name resolution should be performed.
     */
    base_connector(const std::string& host, const std::string& port, time_duration_type resolve_timeout,
                   time_duration_type connect_timeout, time_duration_type operation_timeout,
                   ::stream_client::resolver::ip_family ip_family = resolver_type::kDefaultIPFamily,
                   resolve_flags_type resolve_flags = resolver_type::kDefaultFlags);

    /// Copy constructor is not permitted.
    base_connector(const base_connector<Stream>& other) = delete;
    /// Copy assignment is not permitted.
    base_connector<Stream>& operator=(const base_connector<Stream>& other) = delete;
    /// Move constructor is not permitted.
    base_connector(base_connector<Stream>&& other) = delete;
    /// Move assignment is not permitted.
    base_connector<Stream>& operator=(base_connector<Stream>&& other) = delete;

    /// Destructor.
    virtual ~base_connector();

    /**
     * Establish new session.
     * Returns new connected stream. The call will block until one of the following conditions is true:
     *
     * @li Successfully connected to remote.
     * @li An error or timeout occurred. In this case internal DNS resolver triggered to update list of endpoints.
     *
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns New session wrapped in std::unique_ptr or nullptr.
     */
    std::unique_ptr<stream_type> new_session(boost::system::error_code& ec, const time_point_type& deadline);

    /**
     * Establish new session.
     * Returns new connected stream. The call will block until one of the following conditions is true:
     *
     * @li Successfully connected to remote.
     * @li An error or timeout occurred. In this case internal DNS resolver triggered to update list of endpoints.
     *
     * @param[in] timeout Expiration duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns New session wrapped in std::unique_ptr or nullptr.
     */
    inline std::unique_ptr<stream_type> new_session(boost::system::error_code& ec, const time_duration_type& timeout)
    {
        return new_session(ec, clock_type::now() + timeout);
    }

    /**
     * Establish new session.
     * Returns new connected stream. Uses @p connect_timeout passed on construction as timeout value.
     * The call will block until one of the following conditions is true:
     *
     * @li Successfully connected to remote.
     * @li An error or timeout occurred. In this case internal DNS resolver triggered to update list of endpoints.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns New session wrapped in std::unique_ptr or nullptr.
     */
    inline std::unique_ptr<stream_type> new_session(boost::system::error_code& ec)
    {
        return new_session(ec, get_connect_timeout());
    }

    /**
     * Establish new session.
     * Returns new connected stream. Uses @p connect_timeout passed on construction as timeout value.
     * The call will block until one of the following conditions is true:
     *
     * @li Successfully connected to remote.
     * @li An error or timeout occurred. In this case internal DNS resolver triggered to update list of endpoints.
     *
     * @note A successful call to this function is guaranteed to return valid pointer.
     *
     * @returns New session wrapped in std::unique_ptr. Guaranteed to return valid pointer.
     * @throws boost::system::system_error Thrown on failure.
     */
    inline std::unique_ptr<stream_type> new_session()
    {
        boost::system::error_code ec;
        auto session = new_session(ec);
        if (ec) {
            throw boost::system::system_error{ec, "Failed to establish new session to " + get_target()};
        }
        if (!session) {
            throw boost::system::system_error{boost::asio::error::operation_aborted,
                                              "Failed to establish new session to " + get_target()};
        }
        return session;
    }

    /**
     * Get remote hostname.
     *
     * @returns A string representing remote host.
     */
    inline std::string get_host() const
    {
        return host_;
    }

    /**
     * Get remote port.
     *
     * @returns A string representing remote port.
     */
    inline std::string get_port() const
    {
        return port_;
    }

    /**
     * Get remote target in format 'host:port'
     *
     * @returns A string representing remote endpoint.
     */
    inline std::string get_target() const
    {
        return get_host() + ":" + get_port();
    }

    /**
     * Get resolve operations timeout.
     * This function used to get current timeout for DNS resolve operations.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& get_resolve_timeout() const
    {
        return resolve_timeout_;
    }

    /**
     * Get connect operations timeout.
     * This function used to get current timeout used in new_session() by default.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& get_connect_timeout() const
    {
        return connect_timeout_;
    }

    /**
     * Get I/O operations timeout on result streams.
     * This function used to get I/O timeout used in new sessions returned by new_session().
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& get_operation_timeout() const
    {
        return operation_timeout_;
    }

protected:
    /// Try to connect to selected endpoint until deadline is not met.
    virtual std::unique_ptr<stream_type> connect_until(const endpoint_type& peer_endpoint,
                                                       const time_point_type& until_time) const;

    /// Background routine used to obtain and update remote endpoints.
    void resolve_routine();

    /// Thread-safe setter for @p endpoints_.
    inline void update_endpoints(resolver_endpoint_iterator_type&& resolved_endpoints)
    {
        endpoint_container_type new_endpoints;
        while (resolved_endpoints != resolver_endpoint_iterator_type()) {
            new_endpoints.emplace_back(std::move(*resolved_endpoints));
            ++resolved_endpoints;
        }

        std::unique_lock<std::mutex> lk(endpoints_mutex_);
        endpoints_ = std::move(new_endpoints);
    }

    /// Thread-safe getter for @p endpoints_.
    inline endpoint_container_type get_endpoints()
    {
        std::unique_lock<std::mutex> lk(endpoints_mutex_);
        return endpoints_;
    }

    /// Thread-safe setter for @p resolve_error_.
    inline void set_resolve_error(const boost::system::error_code& err)
    {
        std::unique_lock<std::mutex> lk(resolve_error_mutex_);
        resolve_error_ = err;
    }

    /// Thread-safe getter for @p resolve_error_.
    inline boost::system::error_code get_resolve_error()
    {
        std::unique_lock<std::mutex> lk(resolve_error_mutex_);
        return resolve_error_;
    }

    /// Trigger resolve_routine() to update @p endpoints_.
    inline void notify_resolve_needed()
    {
        std::unique_lock<std::timed_mutex> resolve_needed_lk(resolve_needed_mutex_);
        resolve_needed_ = true;
        resolve_needed_lk.unlock();
        resolve_needed_cv_.notify_all();
    }

    /// Notify from resolve_routine() that @p endpoints_ updated.
    inline void notify_resolve_done()
    {
        std::unique_lock<std::timed_mutex> resolve_done_lk(resolve_done_mutex_);
        resolve_done_ = true;
        resolve_done_lk.unlock();
        resolve_done_cv_.notify_all();
    }

private:
    const std::string host_; ///< Remote host.
    const std::string port_; ///< Remote port.
    const time_duration_type resolve_timeout_; ///< Resolve timeout value used in resolve_routine().
    const time_duration_type connect_timeout_; ///< Timeout for connecting operation, used by new_session().
    const time_duration_type operation_timeout_; ///< Timeout for I/O operations on established sessions.

    resolver_type resolver_; ///< Underlying resolver.

    endpoint_container_type endpoints_; ///< Resolved endpoints.
    mutable std::mutex endpoints_mutex_; ///< @p endpoints_ mutex.

    std::atomic_bool resolving_thread_running_{true}; ///< Flag to stop @p resolving_thread_.
    std::thread resolving_thread_; ///< Thread to run resolve_routine() in.

    boost::system::error_code resolve_error_; ///< Error value propagated from @p resolving_thread_.
    mutable std::mutex resolve_error_mutex_; ///< @p resolve_error_ mutex.

    /* used to implement waits on update/done events without polling */
    bool resolve_needed_{true}; ///< Flag to trigger @p resolving_thread_ to reresolve.
    std::timed_mutex resolve_needed_mutex_; ///< @p resolve_needed_ mutex.
    std::condition_variable_any resolve_needed_cv_; ///< @p resolve_needed_ condition variable.
    bool resolve_done_{false}; ///< Flag to notify that @p resolving_thread_ has done @p endpoints_ update.
    std::timed_mutex resolve_done_mutex_; ///< @p resolve_done_ mutex.
    std::condition_variable_any resolve_done_cv_; ///< @p resolve_done_ condition variable.
};

//! Connector for plain TCP protocol.
using tcp_connector = base_connector<::stream_client::tcp_client>;
//! Connector for plain UDP protocol.
using udp_connector = base_connector<::stream_client::udp_client>;

//! Connector for encrypted TCP protocol.
using ssl_connector = base_connector<::stream_client::ssl::ssl_client>;

//! Connector for HTTP protocol.
using http_connector = base_connector<::stream_client::http::http_client>;
//! Connector for HTTPS protocol.
using https_connector = base_connector<::stream_client::http::https_client>;

} // namespace connector
} // namespace stream_client

#include "impl/connector.ipp"
