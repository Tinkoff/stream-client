#pragma once

#include "stream_socket.hpp"

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/rfc2818_verification.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace stream_client {
namespace ssl {

/**
 * SSL stream layer.
 *
 * This class wraps usage of boost::asio::ssl::context over boost::asio::ssl::stream<Socket>.
 *
 * @note Not thread-safe, multi-thread rw access may mess up your data stream and/or timeout handling.
 *
 * @tparam Socket Type of underlying stream socket to use.
 */
template <typename Socket>
class stream_socket
{
public:
    using next_layer_type = typename std::remove_reference<Socket>::type;
    using protocol_type = typename next_layer_type::protocol_type;
    using native_handle_type = typename boost::asio::ssl::stream<Socket>::native_handle_type;
    using lowest_layer_type = typename next_layer_type::lowest_layer_type;
    using ssl_layer_type = typename boost::asio::ssl::stream<next_layer_type>;

    using endpoint_type = typename next_layer_type::endpoint_type;

    using clock_type = typename next_layer_type::clock_type;
    using time_duration_type = typename next_layer_type::time_duration_type;
    using time_point_type = typename next_layer_type::time_point_type;

    using next_layer_config = const typename next_layer_type::config;

    /**
     * Parametrized constructor.
     * Constructs SSL stream connected to @p peer_endpoint. This operation
     * blocks until stream is constructed or @p connect_timeout time elapsed.
     *
     * @param[in] peer_endpoint Remote endpoint to connect.
     * @param[in] connect_timeout Timeout for connection operation.
     * @param[in] operation_timeout Subsequent I/O operation default timeout.
     * @param[in] upstream_host Hostname to check SSL-cretificate against.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    stream_socket(const endpoint_type& peer_endpoint, time_duration_type connect_timeout,
                  time_duration_type operation_timeout, const std::string& upstream_host,
                  bool rfc2818_handshake = true);

    /// Copy constructor is not permitted.
    stream_socket(const stream_socket<Socket>& other) = delete;
    /// Copy assignment is not permitted.
    stream_socket<Socket>& operator=(const stream_socket<Socket>& other) = delete;
    /// Move constructor.
    stream_socket(stream_socket<Socket>&& other) = default;
    /// Move assignment.
    stream_socket<Socket>& operator=(stream_socket<Socket>&& other) = default;

    /// Destructor.
    virtual ~stream_socket() = default;

    /**
     * Perform SSL shutdown. Reception of close_notify constrained with default I/O timeout of @p next_layer_type.
     *
     * @returns What error occurred, if any.
     */
    boost::system::error_code close();

    /**
     * Perform SSL shutdown. Reception of close_notify constrained with default I/O timeout of @p next_layer_type.
     *
     * @param[in] ec Set to indicate what error occurred, if any.
     */
    inline void close(boost::system::error_code& ec)
    {
        ec = close();
    }

    /**
     * Perform SSL handshaking.
     * This function is used to perform SSL handshaking on the stream.
     * The call will block until handshaking is complete or an error occurs.
     *
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @returns Value of the error occurred, if any (same as @p ec).
     */
    template <typename Time>
    boost::system::error_code handshake(boost::system::error_code& ec, const Time& timeout_or_deadline);

    /**
     * Perform SSL handshaking.
     * This function is used to perform SSL handshaking on the stream.
     * The call will block until handshaking is complete or an error occurs.
     *
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename Time>
    void handshake(const Time& timeout_or_deadline);

    /// Alias to handshake() using current connection timeout value.
    inline void handshake()
    {
        return handshake(connection_timeout());
    }

    /**
     * Send data through the stream.
     * The call will block until one of the following conditions is true:
     *
     * @li All of the data in the supplied buffers has been written.
     * @li An error or timeout occurred.
     *
     * @tparam ConstBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers containing the data to be written.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns The number of bytes transferred.
     */
    template <typename ConstBufferSequence>
    std::size_t send(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                     const time_point_type& deadline);

    /**
     * Send data through the stream.
     * The call will block until one of the following conditions is true:
     *
     * @li All of the data in the supplied buffers has been written.
     * @li An error or timeout occurred.
     *
     * @tparam ConstBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers containing the data to be written.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @returns The number of bytes transferred.
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename ConstBufferSequence>
    std::size_t send(const ConstBufferSequence& buffers, const time_point_type& deadline);

    /// Alias to send() using timeout.
    template <typename ConstBufferSequence>
    inline std::size_t send(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                            const time_duration_type& timeout)
    {
        return send(buffers, ec, clock_type::now() + timeout);
    }
    /// Alias to send() using timeout.
    template <typename ConstBufferSequence>
    inline std::size_t send(const ConstBufferSequence& buffers, const time_duration_type& timeout)
    {
        return send(buffers, clock_type::now() + timeout);
    }
    /// Alias to send() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t send(const ConstBufferSequence& buffers, boost::system::error_code& ec)
    {
        return send(buffers, ec, clock_type::now() + io_timeout());
    }
    /// Alias to send() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t send(const ConstBufferSequence& buffers)
    {
        return send(buffers, clock_type::now() + io_timeout());
    }

    /**
     * Receive data through the stream.
     * The call will block until one of the following conditions is true:
     *
     * @li The supplied buffers are full.
     * @li An error or timeout occurred.
     *
     * @tparam MutableBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers into which the data will be received.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns The number of bytes received.
     */
    template <typename MutableBufferSequence>
    std::size_t receive(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                        const time_point_type& deadline);

    /**
     * Receive data through the stream.
     * The call will block until one of the following conditions is true:
     *
     * @li The supplied buffers are full.
     * @li An error or timeout occurred.
     *
     * @tparam MutableBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers into which the data will be received.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @returns The number of bytes received.
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename MutableBufferSequence>
    std::size_t receive(const MutableBufferSequence& buffers, const time_point_type& deadline);

    /// Alias to send() using timeout.
    template <typename MutableBufferSequence>
    inline std::size_t receive(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                               const time_duration_type& timeout)
    {
        return receive(buffers, ec, clock_type::now() + timeout);
    }
    /// Alias to send() using timeout.
    template <typename MutableBufferSequence>
    inline std::size_t receive(const MutableBufferSequence& buffers, const time_duration_type& timeout)
    {
        return receive(buffers, clock_type::now() + timeout);
    }
    /// Alias to receive() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t receive(const MutableBufferSequence& buffers, boost::system::error_code& ec)
    {
        return receive(buffers, ec, clock_type::now() + io_timeout());
    }
    /// Alias to receive() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t receive(const MutableBufferSequence& buffers)
    {
        return receive(buffers, clock_type::now() + io_timeout());
    }

    /**
     * Send some data on the stream.
     * The call will block until one or more bytes of the data has been sent
     * successfully, or an until error occurs.
     *
     * @tparam ConstBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers containing the data to be written.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns The number of bytes sent. Returns 0 if an error occurred.
     */
    template <typename ConstBufferSequence, typename Time>
    std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                           const Time& timeout_or_deadline);

    /**
     * Send some data on the stream.
     * The call will block until one or more bytes of the data has been sent
     * successfully, or an until error occurs.
     *
     * @tparam ConstBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers containing the data to be written.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @returns The number of bytes sent.
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename ConstBufferSequence, typename Time>
    std::size_t write_some(const ConstBufferSequence& buffers, const Time& timeout_or_deadline);

    /// Alias to write_some() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec)
    {
        return write_some(buffers, ec, io_timeout());
    }
    /// Alias to write_some() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t write_some(const ConstBufferSequence& buffers)
    {
        return write_some(buffers, io_timeout());
    }

    /**
     * Receive some data on the stream.
     * The call will block until one or more bytes of data has been received
     * successfully, or until an error occurs.
     *
     * @tparam MutableBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers into which the data will be received.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns The number of bytes received. Returns 0 if an error occurred.
     */
    template <typename MutableBufferSequence, typename Time>
    std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                          const Time& timeout_or_deadline);

    /**
     * Receive some data on the stream.
     * The call will block until one or more bytes of data has been received
     * successfully, or until an error occurs.
     *
     * @tparam MutableBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers into which the data will be received.
     * @param[in] timeout_or_deadline Expiration time-point or duration.
     *
     * @returns The number of bytes received.
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename MutableBufferSequence, typename Time>
    std::size_t read_some(const MutableBufferSequence& buffers, const Time& timeout_or_deadline);

    /// Alias to read_some() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec)
    {
        return read_some(buffers, ec, io_timeout());
    }
    /// Alias to read_some() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t read_some(const MutableBufferSequence& buffers)
    {
        return read_some(buffers, io_timeout());
    }

    /**
     * Get a const reference to the associated boost::asio::ssl::context.
     *
     * @returns This function returns a const reference to the ssl context.
     */
    inline const boost::asio::ssl::context& ssl_context() const
    {
        return ssl_context_;
    }

    /**
     * Get a reference to the associated boost::asio::ssl::context.
     *
     * @returns This function returns a reference to the ssl context.
     */
    inline boost::asio::ssl::context& ssl_context()
    {
        return ssl_context_;
    }

    /**
     * Get a const reference to the underlying boost::asio::ssl::stream.
     *
     * @returns This function returns a const reference to the underlying boost::asio::ssl::stream.
     */
    inline const ssl_layer_type& ssl_layer() const
    {
        return *ssl_stream_;
    }

    /**
     * Get a reference to the underlying boost::asio::ssl::stream.
     *
     * @returns This function returns a reference to the underlying boost::asio::ssl::stream.
     */
    inline ssl_layer_type& ssl_layer()
    {
        return *ssl_stream_;
    }

    /**
     * Get a const reference to the underlying stream of type @p Socket
     *
     * @returns This function returns a const reference to the underlying stream.
     */
    inline const next_layer_type& next_layer() const
    {
        return ssl_layer().next_layer();
    }

    /**
     * Get a reference to the underlying stream of type @p Socket
     *
     * @returns This function returns a reference to the underlying stream.
     */
    inline next_layer_type& next_layer()
    {
        return ssl_layer().next_layer();
    }

    /**
     * Get a const reference to the lowest layer.
     * This function returns a reference to the lowest layer in a stack of
     * layers, basically an instance of boost::asio::basic_stream_socket.
     *
     * @returns A const reference to the lowest layer in the stack of layers.
     */
    inline const lowest_layer_type& lowest_layer() const
    {
        return ssl_layer().lowest_layer();
    }

    /**
     * Get a reference to the lowest layer.
     * This function returns a reference to the lowest layer in a stack of
     * layers, basically an instance of boost::asio::basic_stream_socket.
     *
     * @returns A reference to the lowest layer in the stack of layers.
     */
    inline lowest_layer_type& lowest_layer()
    {
        return ssl_layer().lowest_layer();
    }

    /**
     * Get the underlying implementation in the native type.
     * This function may be used to obtain the underlying implementation of the
     * context. This is intended to allow access to context functionality that
     * is not otherwise provided.
     *
     * @returns A reference to the lowest layer in the stack of layers.
     */
    inline native_handle_type native_handle()
    {
        return ssl_layer().native_handle();
    }

    /**
     * Get connection timeout.
     * This function used to get connection timeout used at construction.
     *
     * @returns A const reference to the connection timeout.
     */
    inline const time_duration_type& connection_timeout() const
    {
        return next_layer().connection_timeout();
    }

    /**
     * Get I/O operations timeout.
     * This function used to get current value for I/O timeout.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& io_timeout() const
    {
        return next_layer().io_timeout();
    }

    /**
     * Set I/O operations timeout.
     * This function used to set new value for I/O timeout.
     *
     * @returns Previous timeout value.
     */
    inline time_duration_type io_timeout(time_duration_type new_io_timeout)
    {
        return next_layer().io_timeout(std::move(new_io_timeout));
    }

    /**
     * Get current status of I/O timeout.
     * This function used to check if I/O timeout enabled or not.
     *
     * @returns A boolean state of the timeout.
     */
    inline bool io_timeout_enabled() const
    {
        return next_layer().io_timeout_enabled();
    }

    /**
     * Set current status of I/O timeout.
     * This function used to enable or disable I/O timeout. If it's disabled,
     * rw functions without explicit timeout value will use indefinite timeout (blocking mode),
     *
     * @returns Previous state of the timeout.
     */
    inline bool io_timeout_enabled(bool new_mode)
    {
        return next_layer().io_timeout_enabled(new_mode);
    }

    /// Determine whether the underlying stream is open.
    inline bool is_open() const
    {
        return next_layer().is_open();
    }

protected:
    /// Get RAII handler for timeout control over stream.
    template <typename Time>
    inline auto scope_expire(const Time& timeout_or_deadline)
    {
        return next_layer().scope_expire(timeout_or_deadline);
    }

private:
    boost::asio::ssl::context ssl_context_; ///< Current SSL context used in stream communications.
    std::unique_ptr<ssl_layer_type> ssl_stream_; ///< Movable boost::asio::ssl::stream handler
};

//! SSL-encrypted TCP stream.
using ssl_client = stream_socket<::stream_client::tcp_client>;

} // namespace ssl
} // namespace stream_client

#include "impl/ssl_stream_socket.ipp"
