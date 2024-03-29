#pragma once

namespace stream_client {
namespace ssl {

template <typename Socket>
stream_socket<Socket>::stream_socket(const endpoint_type& peer_endpoint, time_duration_type connect_timeout,
                                     time_duration_type operation_timeout, const std::string& upstream_host,
                                     bool rfc2818_handshake)
    : ssl_context_(boost::asio::ssl::context::sslv23)
{
    const auto deadline = clock_type::now() + connect_timeout;

    ssl_context_.set_default_verify_paths();
    ssl_stream_ = std::make_unique<ssl_layer_type>(next_layer_config{peer_endpoint, std::move(connect_timeout),
                                                                     std::move(operation_timeout)},
                                                   ssl_context_);

    if (!SSL_set_tlsext_host_name(ssl_stream_->native_handle(), upstream_host.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        throw boost::system::system_error{ec};
    }

    if (rfc2818_handshake) {
        ssl_stream_->set_verify_mode(boost::asio::ssl::verify_peer);
        ssl_stream_->set_verify_callback(boost::asio::ssl::rfc2818_verification(upstream_host));
        handshake(deadline);
    }
}

template <typename Socket>
boost::system::error_code stream_socket<Socket>::close()
{
    boost::system::error_code ec;

    ssl_layer().shutdown(ec);
// http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
#if BOOST_VERSION >= 106200
    bool ssl_short_read_err = (ec.value() == boost::asio::ssl::error::stream_truncated);
#else // older Boost supports only OpenSSL 1.0, so 1.0-only macro is appropriate
    bool ssl_short_read_err = (ERR_GET_REASON(ec.value()) == SSL_R_SHORT_READ);
#endif

    if (ec.category() == boost::asio::error::get_ssl_category() && ssl_short_read_err) {
        ec.assign(0, ec.category());
    }
    if (ec) {
        return ec;
    }
    lowest_layer().close(ec);
    return ec;
}

/* Wrappers to perform SSL handshaking within ssl::stream */
template <typename Socket>
template <typename Time>
boost::system::error_code stream_socket<Socket>::handshake(boost::system::error_code& ec,
                                                           const Time& timeout_or_deadline)
{
    bool orig_io_timeout_mode = io_timeout_enabled(false);
    const auto expire = scope_expire(timeout_or_deadline);

    auto err = ssl_layer().handshake(boost::asio::ssl::stream_base::client, ec);

    io_timeout_enabled(orig_io_timeout_mode);
    return err;
}

template <typename Socket>
template <typename Time>
void stream_socket<Socket>::handshake(const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    handshake(ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error(ec);
    }
}

template <typename Socket>
template <typename ConstBufferSequence>
std::size_t stream_socket<Socket>::send(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                                        const time_point_type& deadline)
{
    std::size_t transfered_bytes = 0;

    while (transfered_bytes != boost::asio::buffer_size(buffers)) {
        transfered_bytes += write_some(buffers, ec, deadline);
        if (ec) {
            break;
        }
    }
    return transfered_bytes;
}

template <typename Socket>
template <typename ConstBufferSequence>
std::size_t stream_socket<Socket>::send(const ConstBufferSequence& buffers, const time_point_type& deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = send(buffers, ec, deadline);
    if (ec) {
        throw boost::system::system_error{ec, "Socket send() failed"};
    }

    return transfered_bytes;
}

template <typename Socket>
template <typename MutableBufferSequence>
std::size_t stream_socket<Socket>::receive(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                                           const time_point_type& deadline)
{
    std::size_t transfered_bytes = 0;

    while (transfered_bytes != boost::asio::buffer_size(buffers)) {
        transfered_bytes += read_some(buffers, ec, deadline);
        if (ec) {
            break;
        }
    }
    return transfered_bytes;
}

template <typename Socket>
template <typename MutableBufferSequence>
std::size_t stream_socket<Socket>::receive(const MutableBufferSequence& buffers, const time_point_type& deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = receive(buffers, ec, deadline);
    if (ec) {
        throw boost::system::system_error{ec, "Socket receive() failed"};
    }

    return transfered_bytes;
}

/* Wrappers to write some data to an ssl::stream */
template <typename Socket>
template <typename ConstBufferSequence, typename Time>
std::size_t stream_socket<Socket>::write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec,
                                              const Time& timeout_or_deadline)
{
    bool orig_io_timeout_mode = io_timeout_enabled(false);
    const auto expire = scope_expire(timeout_or_deadline);

    std::size_t n_bytes = ssl_layer().write_some(buffers, ec);

    io_timeout_enabled(orig_io_timeout_mode);
    return n_bytes;
}

template <typename Socket>
template <typename ConstBufferSequence, typename Time>
std::size_t stream_socket<Socket>::write_some(const ConstBufferSequence& buffers, const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = write_some(buffers, ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    return transfered_bytes;
}

/* Wrappers to read some data from an ssl::stream */
template <typename Socket>
template <typename MutableBufferSequence, typename Time>
std::size_t stream_socket<Socket>::read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec,
                                             const Time& timeout_or_deadline)
{
    bool orig_io_timeout_mode = io_timeout_enabled(false);
    const auto expire = scope_expire(timeout_or_deadline);

    std::size_t n_bytes = ssl_layer().read_some(buffers, ec);

    io_timeout_enabled(orig_io_timeout_mode);
    return n_bytes;
}

template <typename Socket>
template <typename MutableBufferSequence, typename Time>
std::size_t stream_socket<Socket>::read_some(const MutableBufferSequence& buffers, const Time& timeout_or_deadline)
{
    boost::system::error_code ec;
    std::size_t transfered_bytes = read_some(buffers, ec, timeout_or_deadline);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    return transfered_bytes;
}

} // namespace ssl
} // namespace stream_client
