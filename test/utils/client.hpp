#pragma once

namespace utils {

template <typename Client>
Client make_client(const typename Client::endpoint_type& peer_endpoint,
                   typename Client::time_duration_type connect_timeout,
                   typename Client::time_duration_type operation_timeout, const std::string&)
{
    return Client(peer_endpoint, connect_timeout, operation_timeout);
}

template <>
::stream_client::ssl::ssl_client
make_client(const typename ::stream_client::ssl::ssl_client::endpoint_type& peer_endpoint,
            typename ::stream_client::ssl::ssl_client::time_duration_type connect_timeout,
            typename ::stream_client::ssl::ssl_client::time_duration_type operation_timeout,
            const std::string& upstream_host)
{
    ::stream_client::ssl::ssl_client client(peer_endpoint, connect_timeout, operation_timeout, upstream_host, false);
    client.ssl_context().load_verify_file(SSL_ROOT_CA);
    client.ssl_layer().set_verify_mode(boost::asio::ssl::verify_peer);
    client.ssl_layer().set_verify_callback(boost::asio::ssl::rfc2818_verification(upstream_host));
    client.handshake();

    return client;
}

template <typename Client>
std::unique_ptr<Client> make_unique_client(const typename Client::endpoint_type& peer_endpoint,
                                           typename Client::time_duration_type connect_timeout,
                                           typename Client::time_duration_type operation_timeout, const std::string&)
{
    return std::make_unique<Client>(peer_endpoint, connect_timeout, operation_timeout);
}

template <>
std::unique_ptr<::stream_client::ssl::ssl_client>
make_unique_client(const typename ::stream_client::ssl::ssl_client::endpoint_type& peer_endpoint,
                   typename ::stream_client::ssl::ssl_client::time_duration_type connect_timeout,
                   typename ::stream_client::ssl::ssl_client::time_duration_type operation_timeout,
                   const std::string& upstream_host)
{
    auto client = std::make_unique<::stream_client::ssl::ssl_client>(peer_endpoint, connect_timeout, operation_timeout,
                                                                     upstream_host, false);
    client->ssl_context().load_verify_file(SSL_ROOT_CA);
    client->ssl_layer().set_verify_mode(boost::asio::ssl::verify_peer);
    client->ssl_layer().set_verify_callback(boost::asio::ssl::rfc2818_verification(upstream_host));
    client->handshake();

    return client;
}

class ssl_connector: public ::stream_client::connector::base_connector<::stream_client::ssl::ssl_client>
{
public:
    using ::stream_client::connector::base_connector<::stream_client::ssl::ssl_client>::base_connector;

private:
    virtual std::unique_ptr<stream_type> connect_until(const endpoint_type& peer_endpoint,
                                                       const time_point_type& until_time) const override
    {
        const time_duration_type connect_timeout{until_time - clock_type::now()};
        auto client = std::make_unique<::stream_client::ssl::ssl_client>(peer_endpoint, connect_timeout,
                                                                         get_operation_timeout(), get_host(), false);
        client->ssl_context().load_verify_file(SSL_ROOT_CA);
        client->ssl_layer().set_verify_mode(boost::asio::ssl::verify_peer);
        client->ssl_layer().set_verify_callback(boost::asio::ssl::rfc2818_verification(get_host()));
        client->handshake();

        return client;
    }
};

} // namespace utils
