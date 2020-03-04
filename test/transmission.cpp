#include "fixtures.hpp"
#include "utils/common.hpp"

TYPED_TEST(ConnectedEnv, Echo)
{
    // echo of send_data.size() bytes
    this->server_session->do_echo(this->send_data.size());

    ASSERT_NO_THROW({
        this->send_bytes =
            this->client_session->send(boost::asio::buffer(this->send_data.data(), this->send_data.size()));
    });
    EXPECT_EQ(this->send_bytes, this->send_data.size());

    ASSERT_NO_THROW({
        this->recv_bytes = this->client_session->receive(boost::asio::buffer(&this->recv_data[0], this->send_bytes));
    });
    EXPECT_EQ(this->recv_bytes, this->send_data.size());
    EXPECT_EQ(this->send_data, this->recv_data);
}

TEST_F(HTTPConnectedEnv, Echo)
{
    this->server_session->do_echo();

    boost::beast::http::request<boost::beast::http::string_body> request(boost::beast::http::verb::post, "localhost",
                                                                         11, "test");
    request.prepare_payload();

    boost::beast::http::response<boost::beast::http::string_body> response;
    ASSERT_NO_THROW({ response = this->client_session->perform(request); });
    EXPECT_EQ(response.result(), boost::beast::http::status::ok);
    EXPECT_EQ(response.version(), request.version());
    EXPECT_EQ(response.body(), request.body());
}

TYPED_TEST(ConnectedEnv, PartialReceive)
{
    using protocol_type = typename TestFixture::protocol_type;
    // echo 1 byte
    this->server_session->do_echo(1);

    // if send_data fits into kernel write buffer op will succeed;
    // if not - op will be split into several writes, therefore send will timed out
    this->send_bytes = this->client_session->send(boost::asio::buffer(this->send_data.data(), this->send_data.size()),
                                                  this->error);
    EXPECT_CODE_ONEOF2(this->error, boost::system::errc::success, boost::asio::error::timed_out);
    if (this->error == boost::asio::error::timed_out) {
        EXPECT_LE(this->send_bytes, this->send_data.size());
    } else {
        EXPECT_EQ(this->send_bytes, this->send_data.size());
    }
    this->recv_bytes = this->client_session->receive(boost::asio::buffer(&this->recv_data[0], this->send_data.size()),
                                                     this->error);

    // receive should read 1 or 0 bytes
    if (typeid(protocol_type) == typeid(boost::asio::ip::udp)) {
        EXPECT_CODE(this->error, boost::system::errc::success);
        EXPECT_EQ(this->recv_bytes, 1);
        EXPECT_EQ(this->send_data[0], this->recv_data[0]);
    } else {
        EXPECT_CODE_ONEOF2(this->error, boost::asio::error::eof, boost::asio::error::timed_out);
        if (this->error == boost::asio::error::eof) {
            EXPECT_EQ(this->recv_bytes, 0);
        } else {
            EXPECT_EQ(this->recv_bytes, 1);
            EXPECT_EQ(this->send_data[0], this->recv_data[0]);
        }
    }
}

TYPED_TEST(ConnectedEnv, ClosedSend)
{
    // close only server side
    EXPECT_CODE(this->server_session->close(), boost::system::errc::success);

    // if send_data is short, kernel may happily put it into buffer and report successful write;
    // if send_data is too long or the socket has been put into closed state, send will fail
    this->send_bytes = this->client_session->send(boost::asio::buffer(this->send_data.data(), this->send_data.size()),
                                                  this->error);
    EXPECT_CODE_ONEOF3(this->error, boost::system::errc::success, boost::system::errc::wrong_protocol_type,
                       boost::asio::error::broken_pipe);
    if (this->error == boost::system::errc::success) {
        EXPECT_EQ(this->send_bytes, this->send_data.size());
    } else {
        EXPECT_LE(this->send_bytes, this->send_data.size());
    }
}

TYPED_TEST(ConnectedEnv, ClosedReceive)
{
    using protocol_type = typename TestFixture::protocol_type;
    // echo 1 byte, then close the socket
    this->server_session->do_echo(1, true);

    this->send_bytes = this->client_session->send(boost::asio::buffer(this->send_data.data(), this->send_data.size()),
                                                  this->error);
    EXPECT_LE(this->send_bytes, this->send_data.size());
    EXPECT_GE(this->send_bytes, 1);

    this->recv_bytes = this->client_session->receive(boost::asio::buffer(&this->recv_data[0], this->send_bytes),
                                                     this->error);
    // receive should read 1 byte
    if (typeid(protocol_type) == typeid(boost::asio::ip::udp)) {
        EXPECT_CODE(this->error, boost::system::errc::success);
    } else {
        EXPECT_CODE_ONEOF3(this->error, boost::asio::error::eof, boost::asio::error::connection_reset,
                           ssl_short_read_err);
    }

    EXPECT_EQ(this->recv_bytes, 1);
    EXPECT_EQ(this->send_data[0], this->recv_data[0]);

    EXPECT_CODE_ONEOF2(this->client_session->close(), boost::system::errc::success, ssl_short_read_err);
    this->recv_bytes = this->client_session->receive(boost::asio::buffer(&this->recv_data[0], this->send_data.size()),
                                                     this->error);
    EXPECT_CODE_ONEOF3(this->error, boost::asio::error::bad_descriptor, boost::asio::error::eof, ssl_short_read_err);
    EXPECT_EQ(this->recv_bytes, 0);
}

TEST_F(TCPConnectedEnv, PartialReadSome)
{
    // echo 1 byte
    this->server_session->do_echo(1);

    // write_some succeeds with at least 1 byte written
    this->send_bytes =
        this->client_session->write_some(boost::asio::buffer(this->send_data.data(), this->send_data.size()),
                                         this->error);
    ASSERT_CODE(this->error, boost::system::errc::success);
    EXPECT_GE(this->send_bytes, 1);
    EXPECT_LE(this->send_bytes, this->send_data.size());

    // read_some succeeds with 1 byte read
    this->recv_bytes = this->client_session->read_some(boost::asio::buffer(&this->recv_data[0], this->send_bytes),
                                                       this->error);
    ASSERT_CODE(this->error, boost::system::errc::success);
    EXPECT_EQ(this->recv_bytes, 1);
    EXPECT_EQ(this->send_data[0], this->recv_data[0]);
}

TEST_F(TCPConnectedEnv, ClosedWriteSome)
{
    // close only server side
    EXPECT_CODE(this->server_session->close(), boost::system::errc::success);

    // if this->send_data fits into kernel buffer, write_some will succeed - triggering CLOSE WAIT/LAST-ACK/CLOSED
    // transition; else write_some should end abruptly
    this->send_bytes =
        this->client_session->write_some(boost::asio::buffer(this->send_data.data(), this->send_data.size()),
                                         this->error);
    EXPECT_CODE_ONEOF3(this->error, boost::system::errc::success, boost::system::errc::wrong_protocol_type,
                       boost::asio::error::broken_pipe);
    EXPECT_LE(this->send_bytes, this->send_data.size());
}

TEST_F(TCPConnectedEnv, ClosedReadSome)
{
    // echo 1 byte, then close the socket
    this->server_session->do_echo(1, true);

    this->send_bytes =
        this->client_session->write_some(boost::asio::buffer(this->send_data.data(), this->send_data.size()),
                                         this->error);
    EXPECT_LE(this->send_bytes, this->send_data.size());
    EXPECT_GE(this->send_bytes, 1);

    this->recv_bytes = this->client_session->read_some(boost::asio::buffer(&this->recv_data[0], this->send_bytes),
                                                       this->error);
    // read_some succeeds with 1 byte read
    ASSERT_CODE(this->error, boost::system::errc::success);
    EXPECT_EQ(this->recv_bytes, 1);
    EXPECT_EQ(this->send_data[0], this->recv_data[0]);

    EXPECT_CODE(this->client_session->close(), boost::system::errc::success);
    this->recv_bytes = this->client_session->read_some(boost::asio::buffer(&this->recv_data[0], this->send_data.size()),
                                                       this->error);
    EXPECT_CODE_ONEOF2(this->error, boost::asio::error::bad_descriptor, boost::asio::error::eof);
    EXPECT_EQ(this->recv_bytes, 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
