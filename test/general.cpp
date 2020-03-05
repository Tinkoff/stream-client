#include "fixtures.hpp"
#include "utils/common.hpp"

TYPED_TEST(ServerEnv, Connect)
{
    using client_type = typename TestFixture::client_type;
    using endpoint_type = typename TestFixture::endpoint_type;
    using protocol_type = typename TestFixture::protocol_type;

    endpoint_type endpoint;
    std::unique_ptr<client_type> client_session;
    // default (not valid) endpoint should not get connected
    if (typeid(protocol_type) == typeid(boost::asio::ip::udp)) {
#if defined(__linux__)
        // udp on linux will get connected anyway
        ASSERT_NO_THROW({
            client_session = utils::make_unique_client<client_type>(endpoint, std::chrono::seconds(1),
                                                                    std::chrono::seconds(1), "localhost");
        });
#else
        ASSERT_THROW(utils::make_client<client_type>(endpoint, std::chrono::seconds(1), std::chrono::seconds(1),
                                                     "localhost"),
                     boost::system::system_error);
#endif
    } else {
        ASSERT_THROW(utils::make_client<client_type>(endpoint, std::chrono::seconds(1), std::chrono::seconds(1),
                                                     "localhost"),
                     boost::system::system_error);
    }

    // valid (listened) endpoint should get connected
    auto future_session = this->server.get_session();
    ASSERT_NO_THROW({
        client_session = utils::make_unique_client<client_type>(this->server_endpoint, std::chrono::seconds(1),
                                                                std::chrono::seconds(1), "localhost");
    });

    auto server_session = future_session.get();
    EXPECT_TRUE(client_session->is_open());
    EXPECT_CODE(server_session.close(), boost::system::errc::success);
    // we don't perform proper SSL shutdown on server side
    EXPECT_CODE_ONEOF3(client_session->close(), boost::system::errc::success, boost::asio::error::connection_reset,
                       ssl_short_read_err);
}

TYPED_TEST(ConnectedEnv, ClosedOps)
{
    this->server_session->close();
    this->client_session->close();

    this->InitData(1024);
    ASSERT_THROW(this->client_session->send(boost::asio::buffer(this->send_data.data(), this->send_data.size())),
                 boost::system::system_error);

    ASSERT_THROW(this->client_session->receive(boost::asio::buffer(&this->recv_data[0], this->send_data.size())),
                 boost::system::system_error);
}

TYPED_TEST(ConnectedEnv, DoubleClose)
{
    EXPECT_CODE(this->server_session->close(), boost::system::errc::success);
    EXPECT_CODE_ONEOF3(this->client_session->close(), boost::system::errc::success,
                       boost::asio::error::connection_reset, ssl_short_read_err);
    EXPECT_CODE_ONEOF3(this->client_session->close(), boost::system::errc::success, boost::asio::error::bad_descriptor,
                       ssl_short_read_err);
}

TYPED_TEST(TCPUDPConnectedEnv, GetSetOption)
{
    boost::asio::socket_base::reuse_address reuse_address(true);
    ASSERT_NO_THROW(this->client_session->set_option(reuse_address));
    reuse_address = false;
    ASSERT_NO_THROW(this->client_session->get_option(reuse_address));
    EXPECT_TRUE(reuse_address.value());

    reuse_address = false;
    ASSERT_NO_THROW(this->client_session->set_option(reuse_address));
    reuse_address = true;
    ASSERT_NO_THROW(this->client_session->get_option(reuse_address));
    EXPECT_FALSE(reuse_address.value());

    // no need to check other options or if option really has been set,
    // since this function is just a wrap of boost::asio::basic_socket::get_option and we don't test boost itself
}

TYPED_TEST(TCPUDPConnectedEnv, Endpoints)
{
    const auto local_endpoint = this->client_session->local_endpoint();
    EXPECT_EQ(local_endpoint.address(), this->server_endpoint.address());
    EXPECT_NE(local_endpoint.port(), this->server_endpoint.port());

    const auto remote_endpoint = this->client_session->remote_endpoint();
    EXPECT_EQ(remote_endpoint, this->server_endpoint);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
