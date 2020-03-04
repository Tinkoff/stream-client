#pragma once

namespace stream_client {

template <typename Socket>
base_socket<Socket>::base_socket(const endpoint_type& peer_endpoint, time_duration_type connect_timeout,
                                 time_duration_type operation_timeout)
    : detail::steady_timed_base()
    , socket_(get_io_service())
    , connection_timeout_(std::move(connect_timeout))
    , io_operation_timeout_(std::move(operation_timeout))
{
    io_timeout_enabled(false);
    const auto expiration = scope_expire(connection_timeout_);

    boost::system::error_code ec = boost::asio::error::would_block;
    socket_.async_connect(peer_endpoint, [&](const boost::system::error_code& error) {
        if (this->deadline_fired_) {
            ec = boost::asio::error::timed_out;
        } else {
            ec = error;
        }
    });
    while (ec == boost::asio::error::would_block) {
        get_io_service().run_one();
    };

    if (ec) {
        if (ec == boost::asio::error::timed_out) {
            throw boost::system::system_error{ec, "Socket connection timed out"};
        }
        throw boost::system::system_error{ec, "Socket connection failed"};
    }
    if (!socket_.is_open()) {
        throw boost::system::system_error{boost::asio::error::operation_aborted, "Socket connection failed"};
    }

    set_option(boost::asio::socket_base::keep_alive(true));
    if (std::is_same<Socket, boost::asio::ip::tcp>::value) {
        set_option(boost::asio::ip::tcp::no_delay(true));
#if __linux__
        set_option(boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_QUICKACK>(true));
#endif
    }

    io_timeout_enabled(true);
}

template <typename Socket>
base_socket<Socket>::base_socket(const config& cfg)
    : base_socket(cfg.peer_endpoint, cfg.connect_timeout, cfg.operation_timeout)
{
}

template <typename Socket>
base_socket<Socket>::~base_socket()
{
    boost::system::error_code ignored_ec;
    close(ignored_ec);
}

template <typename Socket>
boost::system::error_code base_socket<Socket>::close()
{
    boost::system::error_code ec;
    next_layer().close(ec);
    return ec;
}

template <typename Socket>
template <typename SettableSocketOption>
void base_socket<Socket>::set_option(const SettableSocketOption& option)
{
    boost::system::error_code ec;
    set_option(option, ec);
    if (ec) {
        throw boost::system::system_error{ec, "Socket set_option() failed"};
    }
}

template <typename Socket>
template <typename GettableSocketOption>
void base_socket<Socket>::get_option(GettableSocketOption& option) const
{
    boost::system::error_code ec;
    get_option(option, ec);
    if (ec) {
        throw boost::system::system_error{ec, "Socket get_option() failed"};
    }
}

template <typename Socket>
typename base_socket<Socket>::endpoint_type base_socket<Socket>::local_endpoint() const
{
    boost::system::error_code ec;
    auto endpoint = local_endpoint(ec);
    if (ec) {
        throw boost::system::system_error{ec, "Socket local_endpoint() failed"};
    }
    return endpoint;
}

template <typename Socket>
typename base_socket<Socket>::endpoint_type base_socket<Socket>::remote_endpoint() const
{
    boost::system::error_code ec;
    auto endpoint = remote_endpoint(ec);
    if (ec) {
        throw boost::system::system_error{ec, "Socket remote_endpoint() failed"};
    }
    return endpoint;
}

template <typename Socket>
void base_socket<Socket>::deadline_actor()
{
    boost::system::error_code ignored_ec;
    close(ignored_ec);
}

} // namespace stream_client
