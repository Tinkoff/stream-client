#pragma once

#include "stream-client/detail/timed_base.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/basic_resolver_iterator.hpp>
#include <boost/asio/ip/basic_resolver_query.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <thread>

namespace stream_client {
namespace resolver {

enum class ip_family
{
    ipv4,
    ipv6,
    any
};

/**
 * DNS resolver.
 *
 * This class wraps usage of boost::asio::ip::basic_resolver to make it timeout-blocked.
 * Used to resolve a hostname:port pair to an iterator of endpoints.
 *
 * @note Not thread-safe, concurrent calls of resolve() may mess up timeout handling.
 *
 * @tparam Protocol Type of result endpoints.
 */
template <typename Protocol>
class base_resolver: public ::stream_client::detail::steady_timed_base
{
public:
    using protocol_type = typename std::remove_reference<Protocol>::type;
    using resolver_type = boost::asio::ip::basic_resolver<protocol_type>;
    using query_type = typename resolver_type::query;
    using resolve_flags_type = typename query_type::flags;
    using iterator_type = typename resolver_type::iterator;

    /// Default flags to use with internal DNS query.
    static const resolve_flags_type kDefaultFlags = resolve_flags_type::address_configured;
    /// Default version of IP protocol to resolve into.
    static const ip_family kDefaultIPFamily = ip_family::any;

    /**
     * Parametrized constructor.
     * Stores remote parameters needed for resolving.
     * Doesn't do actual resolution, to make it happen call resolve() on constructed instance.
     *
     * @param[in] host Remote hostname.
     * @param[in] port Remote port used in resolved endpoints.
     * @param[in] resolve_timeout Default resolve timeout.
     * @param[in] protocol Family of IP protocol to resolve (ipv4, ipv6 or any).
     * @param[in] resolve_flags A set of flags that determine how name resolution should be performed.
     */
    base_resolver(std::string host, std::string port, time_duration_type resolve_timeout,
                  ip_family protocol = kDefaultIPFamily, resolve_flags_type resolve_flags = kDefaultFlags);

    /// Copy constructor is not permitted.
    base_resolver(const base_resolver<Protocol>& other) = delete;
    /// Copy assignment is not permitted.
    base_resolver<Protocol>& operator=(const base_resolver<Protocol>& other) = delete;
    /// Move constructor.
    base_resolver(base_resolver<Protocol>&& other) = default;
    /// Move assignment.
    base_resolver<Protocol>& operator=(base_resolver<Protocol>&& other) = default;

    /// Destructor.
    virtual ~base_resolver() = default;

    /**
     * Perform DNS resolve.
     * Resolve stored hostname into iterator of address:port pairs within specified timeout.
     *
     * @note A successful call to this function is guaranteed to return at least one entry.
     *
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A forward-only iterator that can be used to traverse the list of endpoint entries.
     *      Returns a default constructed iterator if an error occurs.
     */
    template <typename Time>
    iterator_type resolve(boost::system::error_code& ec, const Time& timeout_or_deadline);

    /**
     * Perform DNS resolve.
     * Resolve stored hostname into iterator of address:port pairs within default timeout.
     * Timeout can be manipulated with resolve_timeout().
     *
     * @note A successful call to this function is guaranteed to return at least one entry.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns A forward-only iterator that can be used to traverse the list of endpoint entries.
     *      Returns a default constructed iterator if an error occurs.
     */
    inline iterator_type resolve(boost::system::error_code& ec)
    {
        return resolve(ec, resolve_timeout());
    }

    /**
     * Perform DNS resolve.
     * Resolve stored hostname into iterator of address:port pairs within default timeout.
     * Timeout can be manipulated with resolve_timeout().
     *
     * @note A successful call to this function is guaranteed to return at least one entry.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    inline iterator_type resolve()
    {
        boost::system::error_code ec;
        auto endpoints = resolve(ec);
        if (ec) {
            throw boost::system::system_error{ec};
        }
        return endpoints;
    }

    /**
     * Get resolve operations timeout.
     * This function used to get current value for resolve() timeout.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& resolve_timeout() const
    {
        return resolve_timeout_;
    }

    /**
     * Set resolve operations timeout.
     * This function used to set new value for resolve() timeout.
     *
     * @returns Previous timeout value.
     */
    inline time_duration_type resolve_timeout(time_duration_type new_resolve_timeout)
    {
        std::swap(resolve_timeout_, new_resolve_timeout);
        return new_resolve_timeout;
    }

private:
    /// Timeout expiration handler which cancels all resolve() operation pending.
    virtual void deadline_actor() override;

    /// Start an asynchronous resolve with timeout.
    template <typename ResolveHandler, typename Time>
    inline void async_resolve(ResolveHandler&& handler, const Time& timeout_or_deadline)
    {
        auto expire = scope_expire(timeout_or_deadline);
        // clang-format off
        resolver_.async_resolve(
            *query_,
            [e = std::move(expire), h = std::forward<ResolveHandler>(handler)](const boost::system::error_code& error,
                                                                               iterator_type iterator)
            {
                h(error, std::move(iterator));
            }
        );
        // clang-format on
    }

    resolver_type resolver_; ///< Underlying resolver.
    time_duration_type resolve_timeout_; ///< Current resolve timeout value.
    std::unique_ptr<query_type> query_; ///< Default-constructable DNS query holder.
};

//! Resolver returning both IPv4 and IPv6 TCP endpoints.
using tcp_resolver = base_resolver<boost::asio::ip::tcp>;
//! Resolver returning both IPv4 and IPv6 UDP endpoints.
using udp_resolver = base_resolver<boost::asio::ip::udp>;

} // namespace resolver
} // namespace stream_client

#include "impl/resolver.ipp"
