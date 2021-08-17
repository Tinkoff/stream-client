#pragma once

#include "detail/static_allocator.hpp"
#include "dgram_socket.hpp"
#include "ssl_stream_socket.hpp"
#include "stream_socket.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

namespace stream_client {
namespace http {

/**
 * HTTP stream layer.
 *
 * This class wraps usage of boost::beast::http::request_serializer and
 * boost::beast::http::response_parser over Stream. If you want HTTPS use
 * stream_client::ssl::tcp_client type for underlying stream.
 *
 * @note Not thread-safe, multi-thread rw access may mess up your data stream
 * and/or timeout handling.
 *
 * @tparam Stream Type od underlying stream to use.
 */
template <typename Stream>
class base_socket
{
public:
    /// Headers length limit for internal parser
    static const size_t kHeaderLimit;
    /// Body length limit for internal parser
    static const size_t kBodyLimit;

    using allocator_type = ::stream_client::stream::detail::static_allocator<char>;
    using next_layer_type = typename std::remove_reference<Stream>::type;
    using protocol_type = typename next_layer_type::protocol_type;
    using endpoint_type = typename next_layer_type::endpoint_type;
    using clock_type = typename next_layer_type::clock_type;
    using time_duration_type = typename next_layer_type::time_duration_type;
    using time_point_type = typename next_layer_type::time_point_type;

    /**
     * Parametrized constructor.
     * Constructs basic HTTTP stream. This operation blocks until stream is
     * constructed. Passed arguments forwarded to @p Stream constructor.
     *
     * @tparam Arg1 Type of the first argument.
     * @tparam ...ArgN Types of next arguments.
     *
     * @param[in] arg1 First argument to pass to @p Stream constructor.
     * @param[in] ...argn Next arguments to pass to @p Stream constructor.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    template <class Arg1, class... ArgN,
              class = typename std::enable_if<
                  !std::is_same<typename std::decay<Arg1>::type, base_socket<Stream>>::value>::type>
    base_socket(Arg1&& arg1, ArgN&&... argn)
        : stream_(std::forward<Arg1>(arg1), std::forward<ArgN>(argn)...)
        , buffer_allocator_(kBodyLimit + kHeaderLimit)
        , buffer_(kBodyLimit + kHeaderLimit, buffer_allocator_)
    {
    }

    /// Copy constructor is not permitted.
    base_socket(const base_socket<Stream>& other) = delete;
    /// Copy assignment is not permitted.
    base_socket<Stream>& operator=(const base_socket<Stream>& other) = delete;
    /// Move constructor.
    base_socket(base_socket<Stream>&& other) = default;
    /// Move assignment.
    base_socket<Stream>& operator=(base_socket<Stream>&& other) = default;

    /// Destructor.
    virtual ~base_socket() = default;

    /**
     * Perform HTTP request.
     * This function is used to send a request and wait&receive a response until specified deadline is met.
     *
     * @tparam Body Type of the body used in the @p request.
     * @tparam Fields Type of the fields used in the @p request.
     *
     * @param[in] request HTTP request to send.
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Received response wrapped in boost::optional or boost::none if an error occurred.
     */
    template <typename Body, typename Fields>
    boost::optional<boost::beast::http::response<Body, Fields>>
    perform(const boost::beast::http::request<Body, Fields>& request, boost::system::error_code& ec,
            const time_point_type& deadline);

    /**
     * Perform HTTP request.
     * This function is used to send a request and wait&receive a response within specified time.
     *
     * @tparam Body Type of the body used in the @p request.
     * @tparam Fields Type of the fields used in the @p request.
     *
     * @param[in] request HTTP request to send.
     * @param[in] timeout Expiration duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Received response wrapped in boost::optional or boost::none if an error occurred.
     */
    template <typename Body, typename Fields>
    inline boost::optional<boost::beast::http::response<Body, Fields>>
    perform(const boost::beast::http::request<Body, Fields>& request, boost::system::error_code& ec,
            const time_duration_type& timeout)
    {
        return perform(request, ec, clock_type::now() + timeout);
    }

    /**
     * Perform HTTP request.
     * This function is used to send a request and wait&receive a response.
     * Timeout for this operation is defined by stream's I/O default timeout.
     *
     * @tparam Body Type of the body used in the @p request.
     * @tparam Fields Type of the fields used in the @p request.
     *
     * @param[in] request HTTP request to send.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Received response wrapped in boost::optional or boost::none if an error occurred.
     */
    template <typename Body, typename Fields>
    inline boost::optional<boost::beast::http::response<Body, Fields>>
    perform(const boost::beast::http::request<Body, Fields>& request, boost::system::error_code& ec)
    {
        return perform(request, ec, stream_.io_timeout());
    }

    /**
     * Perform HTTP request.
     * This function is used to send a request and wait&receive a response.
     * Timeout for this operation is defined by stream's I/O default timeout.
     *
     * @tparam Body Type of the body used in the @p request.
     * @tparam Fields Type of the fields used in the @p request.
     *
     * @param[in] request HTTP request to send.
     *
     * @returns Received response.
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename Body, typename Fields>
    inline boost::beast::http::response<Body, Fields> perform(const boost::beast::http::request<Body, Fields>& request)
    {
        boost::system::error_code ec;
        auto response = perform(request, ec);
        if (ec) {
            throw boost::system::system_error{ec};
        }
        if (!response) {
            throw boost::system::system_error{boost::asio::error::operation_aborted};
        }
        return *response;
    }

    /**
     * Get a const reference to the underlying stream of type @p Socket
     *
     * @returns This function returns a const reference to the underlying
     * stream.
     */
    inline const next_layer_type& next_layer() const
    {
        return stream_;
    }

    /**
     * Get a reference to the underlying stream of type @p Socket
     *
     * @returns This function returns a reference to the underlying stream.
     */
    inline next_layer_type& next_layer()
    {
        return stream_;
    }

    /// Determine whether the underlying stream is open.
    inline bool is_open() const
    {
        return next_layer().is_open();
    }

protected:
    /**
     * Send HTTP request.
     * This function is used to send a request within defined deadline.
     * Implemented inside as multiple calls of write_some() on the stream.
     *
     * @tparam Body Type of the body used in the @p request.
     * @tparam Fields Type of the fields used in the @p request.
     *
     * @param[in] request HTTP request to send.
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Received response wrapped in boost::optional or boost::none if an error occurred.
     */
    template <typename Body, typename Fields>
    void send_request(const boost::beast::http::request<Body, Fields>& request, boost::system::error_code& ec,
                      const time_point_type& deadline);

    /**
     * Receive HTTP response.
     * This function is used to fill @p response_parser with received response within defined deadline.
     * Implemented inside as multiple calls of read_some() on the stream.
     *
     * @tparam Parser Type of response parse.
     * @tparam DynamicBuffer Type of the buffer used to store read data.
     *
     * @param[in] response_parser HTTP request parser to use.
     * @param[in] buffer Buffer for incoming data.
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     */
    template <typename Parser, typename DynamicBuffer>
    void recv_response(Parser& response_parser, DynamicBuffer& buffer, boost::system::error_code& ec,
                       const time_point_type& deadline);

    /**
     * Receive HTTP response using internal parser.
     * Parser uses intenal allocator for fast operations, but it's limited with @p kHeaderLimit and @p kBodyLimit.
     *
     * @tparam Parser Type of response parse.
     * @tparam DynamicBuffer Type of the buffer used to store read data.
     *
     * @param[in] deadline Expiration time-point.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Received response wrapped in boost::optional or boost::none if an error occurred.
     */
    template <typename Body, typename Fields>
    boost::optional<boost::beast::http::response<Body, Fields>> recv_response(boost::system::error_code& ec,
                                                                              const time_point_type& deadline);

private:
    Stream stream_; ///< Stream to perform rw access.
    allocator_type buffer_allocator_; ///< Allocator used for incoming data.
    boost::beast::basic_flat_buffer<allocator_type> buffer_; ///< Buffer to store incoming data.
};

//! HTTP stream.
using http_client = base_socket<::stream_client::tcp_client>;
//! HTTPS stream.
using https_client = base_socket<::stream_client::ssl::ssl_client>;

} // namespace http
} // namespace stream_client

#include "impl/http_socket.ipp"
