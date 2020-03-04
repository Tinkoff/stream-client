#pragma once

#include "base_socket.hpp"

#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace stream_client {

/**
 * Stream socket client for arbitrary plain (no encryption) protocol.
 *
 * This class wraps boost::asio::basic_stream_socket to make it
 * timeout-blocked: all read/write operations has timeout/deadline option.
 *
 * @note Not thread-safe, multi-thread rw access may mess up your data stream
 * and/or timeout handling.
 *
 * @tparam Protocol Type of protocol to use, see boost::asio::basic_stream_socket.
 */
template <typename Protocol>
class stream_socket: public base_socket<boost::asio::basic_stream_socket<Protocol>>
{
public:
    using base_socket<boost::asio::basic_stream_socket<Protocol>>::base_socket;

    /// Copy constructor is not permitted.
    stream_socket(const stream_socket<Protocol>& other) = delete;
    /// Copy assignment is not permitted.
    stream_socket<Protocol>& operator=(const stream_socket<Protocol>& other) = delete;
    /// Move constructor.
    stream_socket(stream_socket<Protocol>&& other) = default;
    /// Move assignment.
    stream_socket<Protocol>& operator=(stream_socket<Protocol>&& other) = default;

    /// Destructor.
    virtual ~stream_socket() = default;

    /**
     * Close the socket. Wraps boost::asio::basic_stream_socket::shutdown and close.
     * This function is used to close the socket. Any send or receive operations
     * will be canceled immediately, and will complete with the
     * boost::asio::error::operation_aborted error.
     *
     * @returns What error occurred, if any.
     */
    boost::system::error_code close();

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
    template <typename ConstBufferSequence, typename Time>
    std::size_t send(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                     const Time& timeout_or_deadline);

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
    template <typename ConstBufferSequence, typename Time>
    std::size_t send(const ConstBufferSequence& buffers, const Time& timeout_or_deadline);

    /// Alias to send() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t send(const ConstBufferSequence& buffers, boost::system::error_code& ec)
    {
        return send(buffers, ec, this->io_timeout());
    }
    /// Alias to send() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t send(const ConstBufferSequence& buffers)
    {
        return send(buffers, this->io_timeout());
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
    template <typename MutableBufferSequence, typename Time>
    std::size_t receive(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                        const Time& timeout_or_deadline);

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
    template <typename MutableBufferSequence, typename Time>
    std::size_t receive(const MutableBufferSequence& buffers, const Time& timeout_or_deadline);

    /// Alias to receive() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t receive(const MutableBufferSequence& buffers, boost::system::error_code& ec)
    {
        return receive(buffers, ec, this->io_timeout());
    }
    /// Alias to receive() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t receive(const MutableBufferSequence& buffers)
    {
        return receive(buffers, this->io_timeout());
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
        return write_some(buffers, ec, this->io_timeout());
    }
    /// Alias to write_some() using current I/O timeout value.
    template <typename ConstBufferSequence>
    inline std::size_t write_some(const ConstBufferSequence& buffers)
    {
        return write_some(buffers, this->io_timeout());
    }

    /**
     * Receive some data on the stream.
     * The call will block until one or more bytes of data has been received
     * successfully, or until an error occurs.
     *
     * @tparam MutableBufferSequence Type of buffers, see boost::asio::buffer.
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     *
     * @param[in] buffers One or more buffers into which the data will be
     * received.
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
     * @param[in] buffers One or more buffers into which the data will be
     * received.
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
        return read_some(buffers, ec, this->io_timeout());
    }
    /// Alias to read_some() using current I/O timeout value.
    template <typename MutableBufferSequence>
    inline std::size_t read_some(const MutableBufferSequence& buffers)
    {
        return read_some(buffers, this->io_timeout());
    }

private:
    /// Start an asynchronous send with timeout on the underlying socket.
    template <typename ConstBufferSequence, typename WriteHandler, typename Time>
    void async_send(const ConstBufferSequence& buffers, const Time& timeout_or_deadline, WriteHandler&& handler,
                    bool setup_expiration);

    /// Start an asynchronous receive with timeout on the underlying socket.
    template <typename MutableBufferSequence, typename ReadHandler, typename Time>
    void async_receive(const MutableBufferSequence& buffers, const Time& timeout_or_deadline, ReadHandler&& handler,
                       bool setup_expiration);

    /// Start an asynchronous write_some with timeout on the underlying socket.
    template <typename ConstBufferSequence, typename WriteHandler, typename Time>
    void async_write_some(const ConstBufferSequence& buffers, const Time& timeout_or_deadline, WriteHandler&& handler);

    /// Start an asynchronous read_some with timeout on the underlying socket.
    template <typename MutableBufferSequence, typename ReadHandler, typename Time>
    void async_read_some(const MutableBufferSequence& buffers, const Time& timeout_or_deadline, ReadHandler&& handler);
};

//! Plain TCP stream.
using tcp_client = stream_socket<boost::asio::ip::tcp>;

} // namespace stream_client

#include "impl/stream_socket.ipp"
