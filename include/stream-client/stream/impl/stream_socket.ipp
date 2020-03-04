#pragma once

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace stream_client {

template <typename Protocol>
boost::system::error_code stream_socket<Protocol>::close()
{
    boost::system::error_code ec;
    // try gracefully
    this->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    // ignore not_connected since it means it was closed on other side and there is no need to shutdown
    if (ec != boost::system::errc::not_connected) {
        return ec;
    }
    this->next_layer().close(ec);
    return ec;
}

template <typename Protocol>
template <typename ConstBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::send(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                                          const Time& timeout_or_deadline)
{
    std::size_t transfered_bytes = 0;
    bool setup_expiration = true;

    do {
        ec = boost::asio::error::would_block;
        async_send(
            buffers, timeout_or_deadline,
            [&](const boost::system::error_code& error, std::size_t n_bytes) {
                transfered_bytes += n_bytes;
                if (this->deadline_fired_) {
                    ec = boost::asio::error::timed_out;
                } else {
                    ec = error;
                }
            },
            setup_expiration);

        setup_expiration = false;
        while (ec == boost::asio::error::would_block) {
            this->get_io_service().run_one();
        }
    } while (ec == boost::asio::error::try_again);

    this->get_io_service().reset();
    return transfered_bytes;
}

template <typename Protocol>
template <typename ConstBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::send(const ConstBufferSequence& buffers, const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = send(buffers, ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error{ec, "Socket send() failed"};
    }

    return transfered_bytes;
}

template <typename Protocol>
template <typename MutableBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::receive(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                                             const Time& timeout_or_deadline)
{
    std::size_t transfered_bytes = 0;
    bool setup_expiration = true;

    do {
        ec = boost::asio::error::would_block;
        async_receive(
            buffers, timeout_or_deadline,
            [&](const boost::system::error_code& error, std::size_t n_bytes) {
                transfered_bytes += n_bytes;
                if (this->deadline_fired_) {
                    ec = boost::asio::error::timed_out;
                } else {
                    ec = error;
                }
            },
            setup_expiration);

        setup_expiration = false;
        while (ec == boost::asio::error::would_block) {
            this->get_io_service().run_one();
        }
    } while (ec == boost::asio::error::try_again);

    this->get_io_service().reset();
    return transfered_bytes;
}

template <typename Protocol>
template <typename MutableBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::receive(const MutableBufferSequence& buffers, const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = receive(buffers, ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error{ec, "Socket receive() failed"};
    }

    return transfered_bytes;
}

template <typename Protocol>
template <typename ConstBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                                                const Time& timeout_or_deadline)
{
    std::size_t transfered_bytes = 0;
    ec = boost::asio::error::would_block;

    async_write_some(buffers, timeout_or_deadline, [&](const boost::system::error_code& error, std::size_t n_bytes) {
        transfered_bytes = n_bytes;
        if (this->deadline_fired_) {
            ec = boost::asio::error::timed_out;
        } else {
            ec = error;
        }
    });

    while (ec == boost::asio::error::would_block) {
        this->get_io_service().run_one();
    };
    this->get_io_service().reset();
    return transfered_bytes;
}

template <typename Protocol>
template <typename ConstBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::write_some(const ConstBufferSequence& buffers, const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = write_some(buffers, ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error{ec, "Socket write_some() failed"};
    }

    return transfered_bytes;
}

template <typename Protocol>
template <typename MutableBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                                               const Time& timeout_or_deadline)
{
    std::size_t transfered_bytes = 0;
    ec = boost::asio::error::would_block;

    async_read_some(buffers, timeout_or_deadline, [&](const boost::system::error_code& error, std::size_t n_bytes) {
        transfered_bytes = n_bytes;
        if (this->deadline_fired_) {
            ec = boost::asio::error::timed_out;
        } else {
            ec = error;
        }
    });

    while (ec == boost::asio::error::would_block) {
        this->get_io_service().run_one();
    };
    this->get_io_service().reset();
    return transfered_bytes;
}

template <typename Protocol>
template <typename MutableBufferSequence, typename Time>
std::size_t stream_socket<Protocol>::read_some(const MutableBufferSequence& buffers, const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = read_some(buffers, ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error{ec, "Socket read_some() failed"};
    }

    return transfered_bytes;
}

template <typename Protocol>
template <typename ConstBufferSequence, typename WriteHandler, typename Time>
void stream_socket<Protocol>::async_send(const ConstBufferSequence& buffers, const Time& timeout_or_deadline,
                                         WriteHandler&& handler, bool setup_expiration)
{
    typename stream_socket<Protocol>::expiration expire;
    if (setup_expiration) {
        expire = this->scope_expire(timeout_or_deadline);
    }
    // clang-format off
    boost::asio::async_write(
        this->next_layer(), buffers,
        [e = std::move(expire), h = std::forward<WriteHandler>(handler)](const boost::system::error_code& ec,
                                                                         std::size_t n_bytes)
        {
            h(ec, std::move(n_bytes));
        }
    );
    // clang-format on
}

template <typename Protocol>
template <typename MutableBufferSequence, typename ReadHandler, typename Time>
void stream_socket<Protocol>::async_receive(const MutableBufferSequence& buffers, const Time& timeout_or_deadline,
                                            ReadHandler&& handler, bool setup_expiration)
{
    typename stream_socket<Protocol>::expiration expire;
    if (setup_expiration) {
        expire = this->scope_expire(timeout_or_deadline);
    }
    // clang-format off
    boost::asio::async_read(
        this->next_layer(), buffers,
        [e = std::move(expire), h = std::forward<ReadHandler>(handler)](const boost::system::error_code& ec,
                                                                        std::size_t n_bytes)
        {
            h(ec, std::move(n_bytes));
        }
    );
    // clang-format on
}

template <typename Protocol>
template <typename ConstBufferSequence, typename WriteHandler, typename Time>
void stream_socket<Protocol>::async_write_some(const ConstBufferSequence& buffers, const Time& timeout_or_deadline,
                                               WriteHandler&& handler)
{
    auto expire = this->scope_expire(timeout_or_deadline);
    // clang-format off
    this->next_layer().async_write_some(
        buffers, [e = std::move(expire), h = std::forward<WriteHandler>(handler)](const boost::system::error_code& ec,
                                                                                  std::size_t n_bytes)
        {
            h(ec, std::move(n_bytes));
        }
    );
    // clang-format on
}

template <typename Protocol>
template <typename MutableBufferSequence, typename ReadHandler, typename Time>
void stream_socket<Protocol>::async_read_some(const MutableBufferSequence& buffers, const Time& timeout_or_deadline,
                                              ReadHandler&& handler)
{
    auto expire = this->scope_expire(timeout_or_deadline);
    // clang-format off
    this->next_layer().async_read_some(
        buffers, [e = std::move(expire), h = std::forward<ReadHandler>(handler)](const boost::system::error_code& ec,
                                                                                 std::size_t n_bytes)
        {
            h(ec, std::move(n_bytes));
        }
    );
    // clang-format on
}

} // namespace stream_client
