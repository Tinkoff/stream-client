#pragma once

#include "base_socket.hpp"

#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio/ip/udp.hpp>

namespace stream_client {

/**
 * Stream socket client for arbitrary plain (no encryption) protocol.
 *
 * This class wraps boost::asio::basic_datagram_socket to make it
 * timeout-blocked: all read/write operations has timeout/deadline option.
 *
 * @note Not thread-safe, multi-thread rw access may mess up your data stream
 * and/or timeout handling.
 *
 * @tparam Protocol Type of protocol to use, see boost::asio::basic_datagram_socket.
 */
template <typename Protocol>
class datagram_socket: public base_socket<boost::asio::basic_datagram_socket<Protocol>>
{
public:
    using base_socket<boost::asio::basic_datagram_socket<Protocol>>::base_socket;

    /// Copy constructor is not permitted.
    datagram_socket(const datagram_socket<Protocol>& other) = delete;
    /// Copy assignment is not permitted.
    datagram_socket<Protocol>& operator=(const datagram_socket<Protocol>& other) = delete;
    /// Move constructor.
    datagram_socket(datagram_socket<Protocol>&& other) = default;
    /// Move assignment.
    datagram_socket<Protocol>& operator=(datagram_socket<Protocol>&& other) = default;

    /// Destructor.
    virtual ~datagram_socket() = default;

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

private:
    /// Start an asynchronous send with timeout on the underlying socket.
    template <typename ConstBufferSequence, typename WriteHandler, typename Time>
    void async_send(const ConstBufferSequence& buffers, const Time& timeout_or_deadline, WriteHandler&& handler,
                    bool setup_expiration);

    /// Start an asynchronous receive with timeout on the underlying socket.
    template <typename MutableBufferSequence, typename ReadHandler, typename Time>
    void async_receive(const MutableBufferSequence& buffers, const Time& timeout_or_deadline, ReadHandler&& handler,
                       bool setup_expiration);
};

//! Plain UDP stream.
using udp_client = datagram_socket<boost::asio::ip::udp>;

} // namespace stream_client

#include "impl/dgram_socket.ipp"
