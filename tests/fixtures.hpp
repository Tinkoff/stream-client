#pragma once

#include "stream-client/stream-client.hpp"
#include "gtest/gtest.h"

#include "utils/client.hpp"
#include "utils/echo_server.hpp"

template <typename Server>
class ServerEnv: public ::testing::Test
{
public:
    using server_type = Server;
    using session_type = typename server_type::session_type;
    using client_type = typename server_type::client_type;
    using connector_type = typename server_type::connector_type;
    using client_pool_type = typename server_type::client_pool_type;
    using endpoint_type = typename server_type::endpoint_type;
    using protocol_type = typename server_type::protocol_type;

    ServerEnv()
        : ::testing::Test()
        , server_endpoint(boost::asio::ip::address::from_string(address), port)
        , server(server_endpoint)
    {
    }

    const std::string address = "127.0.0.1";
    const std::string host = "localhost";
    const unsigned short port = 6666;
    endpoint_type server_endpoint;
    Server server;
};

using TCPServerEnv = ServerEnv<::utils::tcp_server<1>>;
using UDPServerEnv = ServerEnv<::utils::udp_server>;

template <typename Server>
class ConnectedEnv: public ServerEnv<Server>
{
public:
    using session_type = typename ServerEnv<Server>::session_type;
    using endpoint_type = typename ServerEnv<Server>::endpoint_type;
    using client_type = typename ServerEnv<Server>::client_type;

    ConnectedEnv()
        : ServerEnv<Server>()
        , future_session_(this->server.get_session())
        , connect_timeout(std::chrono::milliseconds(1000))
        , op_timeout(std::chrono::milliseconds(1000))
    {
        client_session = utils::make_unique_client<client_type>(this->server_endpoint, connect_timeout, op_timeout,
                                                                "localhost");
        server_session = std::make_unique<session_type>(std::move(future_session_.get()));
        InitData(9216); // Default MTU for UDP
    }

    void InitData(size_t size)
    {
        for (size_t i = 0; i < size; ++i) {
            send_data += (char)(i % 255);
        }
        recv_data = std::string(size, '\0');
    }

    boost::system::error_code error;
    std::string send_data;
    std::string recv_data;
    size_t send_bytes = 0;
    size_t recv_bytes = 0;

private:
    std::future<session_type> future_session_;

public:
    const std::chrono::milliseconds connect_timeout;
    const std::chrono::milliseconds op_timeout;
    std::unique_ptr<client_type> client_session;
    std::unique_ptr<session_type> server_session;
};

using TCPConnectedEnv = ConnectedEnv<::utils::tcp_server<1>>; // test suite for TCP only
using UDPConnectedEnv = ConnectedEnv<::utils::udp_server>; // test suite for UDP only
using HTTPConnectedEnv = ConnectedEnv<::utils::http_server<1>>; // test suite for HTTP only

template <typename Server>
using TCPUDPConnectedEnv = ConnectedEnv<Server>; // test suite for TCP and UDP

template <typename Server>
class GreedyConnectorPoolServerEnv: public ServerEnv<Server>
{
public:
    using connector_type = typename Server::connector_type;
    using client_pool_type = stream_client::connector::base_connection_pool<connector_type, stream_client::connector::reconnection_strategy_greedy<connector_type>>;
};

template <typename Server>
class ConservativeConnectorPoolServerEnv: public ServerEnv<Server>
{
public:
    using connector_type = typename Server::connector_type;
    using client_pool_type = stream_client::connector::base_connection_pool<connector_type, stream_client::connector::reconnection_strategy_conservative<connector_type>>;
};


using TCPUDPServerTypes = ::testing::Types<::utils::tcp_server<1>, ::utils::udp_server>;
using AllServerTypes = ::testing::Types<::utils::tcp_server<1>, ::utils::udp_server, ::utils::ssl_server<1>>;
using PoolServerTypes =
    ::testing::Types<::utils::tcp_server<boost::asio::ip::tcp::socket::max_connections>, ::utils::udp_server,
                     ::utils::ssl_server<boost::asio::ip::tcp::socket::max_connections>>;
TYPED_TEST_SUITE(ServerEnv, AllServerTypes);
TYPED_TEST_SUITE(ConservativeConnectorPoolServerEnv, PoolServerTypes);
TYPED_TEST_SUITE(GreedyConnectorPoolServerEnv, PoolServerTypes);
TYPED_TEST_SUITE(ConnectedEnv, AllServerTypes);
TYPED_TEST_SUITE(TCPUDPConnectedEnv, TCPUDPServerTypes);

template <typename Resolver>
class ResolverEnv: public ::testing::Test
{
public:
    using resolver_type = Resolver;
    using protocol_type = typename resolver_type::protocol_type;
    using iterator_type = typename resolver_type::iterator_type;

    ResolverEnv()
        : ::testing::Test()
    {
        Init();
    }

    void SetResolver(const std::string& host, unsigned short port,
                     ::stream_client::resolver::ip_family protocol = resolver_type::kDefaultIPFamily)
    {
        resolve_host = host;
        resolve_port = port;
        ip_type = protocol;
        Init();
    }

    void SetResolver(const std::string& host)
    {
        return SetResolver(host, resolve_port);
    }

    void Init()
    {
        resolver = std::make_unique<resolver_type>(resolve_host, std::to_string(resolve_port), resolve_timeout,
                                                   ip_type);
    }

    std::string resolve_host = "localhost";
    unsigned short resolve_port = 6666;
    std::chrono::milliseconds resolve_timeout = std::chrono::milliseconds(500);
    ::stream_client::resolver::ip_family ip_type = resolver_type::kDefaultIPFamily;
    std::unique_ptr<resolver_type> resolver;
};

using ResolverTypes =
    ::testing::Types<::stream_client::resolver::tcp_resolver, ::stream_client::resolver::udp_resolver>;
TYPED_TEST_SUITE(ResolverEnv, ResolverTypes);
