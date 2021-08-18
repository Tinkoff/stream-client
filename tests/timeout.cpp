#include "fixtures.hpp"
#include "utils/common.hpp"

TYPED_TEST(ServerEnv, ConnectZeroTimeout)
{
    using client_type = typename TestFixture::client_type;

    // zero timeout should return immediately with timeout error
    const auto start_t = std::chrono::steady_clock::now();
    ASSERT_THROW_CODE(utils::make_client<client_type>(this->server_endpoint, std::chrono::milliseconds(0),
                                                      std::chrono::seconds(0), "localhost"),
                      boost::asio::error::timed_out);

    const auto elapsed = std::chrono::steady_clock::now() - start_t;
    EXPECT_TIME_LE(elapsed, std::chrono::milliseconds(50)); // actual time differs with sanitized builds
}

TEST_F(TCPServerEnv, ConnectTimeout)
{
    const auto connect_timeout = std::chrono::milliseconds(849);

    // server started with listen backlog = 1 and does not accept the client_session -> backlog occupied
    std::unique_ptr<client_type> client_session1;
    ASSERT_NO_THROW({
        client_session1 = utils::make_unique_client<client_type>(this->server_endpoint, connect_timeout,
                                                                 std::chrono::seconds(0), "localhost");
    });
    EXPECT_TRUE(client_session1->is_open());
    // check connection_timeout() return the same duration
    EXPECT_EQ_DURATION(connect_timeout, client_session1->connection_timeout());

#if !defined(__APPLE__)
    std::unique_ptr<client_type> client_session2;
    ASSERT_NO_THROW({
        client_session2 = utils::make_unique_client<client_type>(this->server_endpoint, connect_timeout,
                                                                 std::chrono::seconds(0), "localhost");
    });
#endif

    // second/third client_session should timed out
    const auto start_t = std::chrono::steady_clock::now();
    ASSERT_THROW_CODE(utils::make_client<client_type>(this->server_endpoint, connect_timeout, std::chrono::seconds(0),
                                                      "localhost"),
                      boost::asio::error::timed_out);
    EXPECT_TIMEOUT_GE(std::chrono::steady_clock::now(), start_t, connect_timeout);
}

// if you know reliable way to simulate send timeout on linux let me know
#if defined(__APPLE__)
TEST_F(TCPConnectedEnv, SendTimeout)
{
    InitData(1 << 20); // arbitrary big chunk of data to cause timeout

    // since server does not accept, transmission should fail
    const auto start_t = std::chrono::steady_clock::now();
    ASSERT_THROW_CODE(
        {
            send_bytes = this->client_session->send(boost::asio::buffer(&this->send_data[0], this->send_data.size()),
                                                    this->op_timeout);
        },
        boost::asio::error::timed_out);
    EXPECT_TIMEOUT_GE(std::chrono::steady_clock::now(), start_t, this->op_timeout);
    EXPECT_EQ(send_bytes, 0);
}

TEST_F(TCPConnectedEnv, SendSetNewTimeout)
{
    InitData(1 << 20); // arbitrary big chunk of data to cause timeout

    // change timeout via io_timeout
    const auto new_op_timeout = this->op_timeout + std::chrono::milliseconds(123);
    const auto old_op_timeout = this->client_session->io_timeout(new_op_timeout);
    EXPECT_EQ_DURATION(old_op_timeout, this->op_timeout); // old returned
    EXPECT_EQ_DURATION(this->client_session->io_timeout(), new_op_timeout); // new has been set

    const auto start_t = std::chrono::steady_clock::now();
    ASSERT_THROW_CODE(this->client_session->send(boost::asio::buffer(&this->send_data[0], this->send_data.size())),
                      boost::asio::error::timed_out);
    EXPECT_TIMEOUT_GE(std::chrono::steady_clock::now(), start_t, new_op_timeout);
}
#endif

TYPED_TEST(ConnectedEnv, ReceiveTimeout)
{
    // since server does not send anything, reception should get timed out
    const auto start_t = std::chrono::steady_clock::now();
    ASSERT_THROW_CODE(
        {
            this->recv_bytes =
                this->client_session->receive(boost::asio::buffer(&this->recv_data[0], this->send_data.size()),
                                              this->op_timeout);
        },
        boost::asio::error::timed_out);
    EXPECT_TIMEOUT_GE(std::chrono::steady_clock::now(), start_t, this->op_timeout);
    EXPECT_EQ(this->recv_bytes, 0);
}

TEST_F(TCPConnectedEnv, ReadSomeTimeout)
{
    // since server does not send anything, reception should get timed out
    const auto start_t = std::chrono::steady_clock::now();
    ASSERT_THROW_CODE(
        {
            this->recv_bytes =
                this->client_session->read_some(boost::asio::buffer(&this->recv_data[0], this->send_data.size()),
                                                this->op_timeout);
        },
        boost::asio::error::timed_out);
    EXPECT_TIMEOUT_GE(std::chrono::steady_clock::now(), start_t, this->op_timeout);
    EXPECT_EQ(this->recv_bytes, 0);
}

TEST_F(HTTPConnectedEnv, PerformTimeout)
{
    this->server_session->do_echo(op_timeout + std::chrono::milliseconds(100));

    boost::beast::http::request<boost::beast::http::string_body> request(boost::beast::http::verb::post, "localhost",
                                                                         11, "test");
    request.prepare_payload();

    const auto start_t = std::chrono::steady_clock::now();
    boost::beast::http::response<boost::beast::http::string_body> response;
    ASSERT_THROW_CODE({ response = this->client_session->perform(request); }, boost::asio::error::timed_out);
    EXPECT_TIMEOUT_GE(std::chrono::steady_clock::now(), start_t, this->op_timeout);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
