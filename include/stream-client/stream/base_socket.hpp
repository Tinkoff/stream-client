#pragma once

#include "stream-client/detail/timed_base.hpp"

namespace stream_client {

/**
 * Socket client for arbitrary plain (no encryption) protocol.
 *
 * This class wraps boost::asio::basic_socket to make it
 * timeout-blocked: all read/write operations has timeout/deadline option.
 *
 * @note Not thread-safe, multi-thread rw access may mess up your data stream
 * and/or timeout handling.
 *
 * @tparam Socket Type of underlying socket to use, see boost::asio::basic_socket.
 */
template <typename Socket>
class base_socket: public detail::steady_timed_base
{
public:
    using next_layer_type = Socket;
    using protocol_type = typename next_layer_type::protocol_type;
    using lowest_layer_type = typename next_layer_type::lowest_layer_type;
    using native_handle_type = typename next_layer_type::native_handle_type;
    using endpoint_type = typename protocol_type::endpoint;

    /// Configuration parameters wrapped into single struct
    struct config
    {
        const endpoint_type& peer_endpoint; ///< Endpoint to connect
        time_duration_type connect_timeout; ///< Connection timeout
        time_duration_type operation_timeout; ///< Any rw function timeout
    };

    /**
     * Parametrized constructor.
     * Constructs socket connected to @p peer_endpoint. This operation blocks
     * until socket is constructed or @p connect_timeout time elapsed.
     *
     * @param[in] peer_endpoint Remote endpoint to connect.
     * @param[in] connect_timeout Timeout for connection operation.
     * @param[in] operation_timeout Subsequent I/O operation default timeout.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    base_socket(const endpoint_type& peer_endpoint, time_duration_type connect_timeout,
                time_duration_type operation_timeout);
    /// Single-argument constructor. Needed to use it as base for boost::asio::ssl::stream.
    base_socket(const config& cfg);

    /// Copy constructor is not permitted.
    base_socket(const base_socket<Socket>& other) = delete;
    /// Copy assignment is not permitted.
    base_socket<Socket>& operator=(const base_socket<Socket>& other) = delete;
    /// Move constructor.
    base_socket(base_socket<Socket>&& other) = default;
    /// Move assignment.
    base_socket<Socket>& operator=(base_socket<Socket>&& other) = default;

    /// Destructor.
    virtual ~base_socket();

    /**
     * Close the socket. Wraps boost::asio::basic_socket::close.
     * This function is used to close the socket. Any send or receive operations
     * will be canceled immediately, and will complete with the
     * boost::asio::error::operation_aborted error.
     *
     * @returns What error occurred, if any.
     */
    boost::system::error_code close();

    /**
     * Close the socket. Wraps boost::asio::basic_socket::close.
     * This function is used to close the socket. Any send or receive operations
     * will be canceled immediately, and will complete with the
     * boost::asio::error::operation_aborted error.
     *
     * @param[in] ec Set to indicate what error occurred, if any.
     */
    inline void close(boost::system::error_code& ec)
    {
        ec = close();
    }

    /**
     * Set an option on the socket.
     *
     * @tparam SettableSocketOption Type of option, see boost::asio 'Socket Options'.
     *
     * @param[in] option The new option value to be set on the socket.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename SettableSocketOption>
    void set_option(const SettableSocketOption& option);

    /**
     * Set an option on the socket.
     *
     * @tparam SettableSocketOption Type of option, see boost::asio 'Socket Options'.
     *
     * @param[in] option The new option value to be set on the socket.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Value of the error occurred, if any (same as @p ec).
     */
    template <typename SettableSocketOption>
    inline boost::system::error_code set_option(const SettableSocketOption& option, boost::system::error_code& ec)
    {
        return next_layer().set_option(option, ec);
    }

    /**
     * Get an option from the socket.
     *
     * @tparam GettableSocketOption Type of option, see boost::asio 'Socket Options'.
     *
     * @param[in] option The option value to be obtained from the socket.
     *
     * @throws boost::system::system_error Thrown on failure.
     */
    template <typename GettableSocketOption>
    void get_option(GettableSocketOption& option) const;

    /**
     * Get an option from the socket.
     *
     * @tparam GettableSocketOption Type of option, see boost::asio 'Socket Options'.
     *
     * @param[in] option The option value to be obtained from the socket.
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns Value of the error occurred, if any (same as @p ec).
     */
    template <typename GettableSocketOption>
    inline boost::system::error_code get_option(GettableSocketOption& option, boost::system::error_code& ec) const
    {
        return next_layer().get_option(option, ec);
    }

    /**
     * Get the local endpoint of the socket.
     *
     * @returns An object that represents the local endpoint of the socket.
     * @throws boost::system::system_error Thrown on failure.
     */
    endpoint_type local_endpoint() const;

    /**
     * Get the local endpoint of the socket.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns An object that represents the local endpoint of the socket.
     */
    inline endpoint_type local_endpoint(boost::system::error_code& ec) const
    {
        return next_layer().local_endpoint(ec);
    }

    /**
     * Get the remote endpoint of the socket.
     *
     * @returns An object that represents the remote endpoint of the socket.
     *      Returns a default-constructed endpoint object if an error occurred.
     * @throws boost::system::system_error Thrown on failure.
     */
    endpoint_type remote_endpoint() const;

    /**
     * Get the remote endpoint of the socket.
     *
     * @param[out] ec Set to indicate what error occurred, if any.
     *
     * @returns An object that represents the remote endpoint of the socket.
     *      Returns a default-constructed endpoint object if an error occurred.
     */
    inline endpoint_type remote_endpoint(boost::system::error_code& ec) const
    {
        return next_layer().remote_endpoint(ec);
    }

    /**
     * Get a const reference to the underlying socket.
     *
     * @returns This function returns a const reference to the underlying socket.
     */
    inline const next_layer_type& next_layer() const
    {
        return socket_;
    }

    /**
     * Get a reference to the underlying socket.
     *
     * @returns This function returns a reference to the underlying socket.
     */
    inline next_layer_type& next_layer()
    {
        return socket_;
    }

    /**
     * Get a const reference to the lowest layer.
     * This function returns a reference to the lowest layer in a stack of
     * layers. Since a boost::asio::basic_socket cannot contain any
     * further layers, it simply returns a reference to underlying socket.
     *
     * @returns A const reference to the lowest layer in the stack of layers.
     */
    inline const lowest_layer_type& lowest_layer() const
    {
        return socket_.lowest_layer();
    }

    /**
     * Get a reference to the lowest layer.
     * This function returns a reference to the lowest layer in a stack of
     * layers. Since a boost::asio::basic_socket cannot contain any
     * further layers, it simply returns a reference to underlying socket.
     *
     * @returns A reference to the lowest layer in the stack of layers.
     */
    inline lowest_layer_type& lowest_layer()
    {
        return socket_.lowest_layer();
    }

    /**
     * Get connection timeout.
     * This function used to get connection timeout used at construction.
     *
     * @returns A const reference to the connection timeout.
     */
    inline const time_duration_type& connection_timeout() const
    {
        return connection_timeout_;
    }

    /**
     * Get I/O operations timeout.
     * This function used to get current value for I/O timeout.
     *
     * @returns A const reference to the timeout.
     */
    inline const time_duration_type& io_timeout() const
    {
        return io_timeouted_ ? io_operation_timeout_ : kInfiniteDuration;
    }

    /**
     * Set I/O operations timeout.
     * This function used to set new value for I/O timeout.
     *
     * @returns Previous timeout value.
     */
    inline time_duration_type io_timeout(time_duration_type new_io_timeout)
    {
        std::swap(io_operation_timeout_, new_io_timeout);
        return new_io_timeout;
    }

    /**
     * Get current status of I/O timeout.
     * This function used to check if I/O timeout enabled or not.
     *
     * @returns A boolean state of the timeout.
     */
    inline bool io_timeout_enabled() const
    {
        return io_timeouted_;
    }

    /**
     * Set current status of I/O timeout.
     * This function used to enable or disable I/O timeout. If it's disabled,
     * rw functions without implicit timeout value will use indefinite timeout (blocking mode).
     *
     * @returns Previous state of the timeout.
     */
    inline bool io_timeout_enabled(bool new_mode)
    {
        std::swap(io_timeouted_, new_mode);
        return new_mode;
    }

    /// Determine whether the underlying socket is open.
    inline bool is_open() const
    {
        return next_layer().is_open();
    }

protected:
    /// Timeout expiration handler which closes current socket.
    virtual void deadline_actor() override;

    /// Start an asynchronous send with timeout on the underlying socket.
    template <typename ConstBufferSequence, typename WriteHandler, typename Time>
    void async_send(const ConstBufferSequence& buffers, const Time& timeout_or_deadline, WriteHandler&& handler,
                    bool setup_expiration);

    /// Start an asynchronous receive with timeout on the underlying socket.
    template <typename MutableBufferSequence, typename ReadHandler, typename Time>
    void async_receive(const MutableBufferSequence& buffers, const Time& timeout_or_deadline, ReadHandler&& handler,
                       bool setup_expiration);

    next_layer_type socket_; ///< Underlying socket to perform I/O operations
    time_duration_type connection_timeout_; ///< Connection timeout value used upon construction
    time_duration_type io_operation_timeout_; ///< Current I/O timeout value
    bool io_timeouted_; ///< Flag to track current I/O timeout state
};

} // namespace stream_client

#include "impl/base_socket.ipp"
