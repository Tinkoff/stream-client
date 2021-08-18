#pragma once

#include <boost/asio.hpp>
#include <future>

namespace utils {

template <typename Socket>
class echo_session
{
public:
    using socket_type = Socket;

    echo_session(std::shared_ptr<boost::asio::io_service> io_service, std::shared_ptr<socket_type> socket)
        : io_service_(std::move(io_service))
        , socket_(std::move(socket))
    {
    }

    echo_session(const echo_session<Socket>& other) = delete;
    echo_session<Socket>& operator=(const echo_session<Socket>& other) = delete;
    echo_session(echo_session<Socket>&& other) = default;
    echo_session<Socket>& operator=(echo_session<Socket>&& other) = default;

    virtual ~echo_session() = default;

protected:
    echo_session() {}

    std::shared_ptr<boost::asio::io_service> io_service_;
    std::shared_ptr<socket_type> socket_;
};

class tcp_session: public echo_session<boost::asio::ip::tcp::socket>
{
public:
    using protocol_type = boost::asio::ip::tcp;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    tcp_session(std::shared_ptr<boost::asio::io_service> io_service, std::shared_ptr<socket_type> socket)
        : echo_session<boost::asio::ip::tcp::socket>(std::move(io_service), std::move(socket))
    {
    }

    tcp_session(const tcp_session& other) = delete;
    tcp_session& operator=(const tcp_session& other) = delete;
    tcp_session(tcp_session&& other) = default;
    tcp_session& operator=(tcp_session&& other) = default;

    virtual ~tcp_session()
    {
        for (auto& t : echo_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void do_echo(const size_t& data_size, const bool& close_after = false)
    {
        echo_threads_.emplace_back([data_size, close_after, this]() {
            size_t recv_bytes = 0;
            boost::system::error_code err_code;
            std::string data(data_size, '\0');

            while (recv_bytes < data_size) {
                recv_bytes += this->socket_->read_some(boost::asio::buffer(&data[recv_bytes], data_size), err_code);
                if (err_code) {
                    if (err_code == boost::asio::error::eof && recv_bytes < data_size) {
                        err_code = boost::asio::error::message_size;
                    }
                    break;
                }
            }

            boost::asio::write(*this->socket_, boost::asio::buffer(&data[0], recv_bytes), err_code);
            if (close_after) {
                this->close();
            }
        });
    }

    boost::system::error_code close()
    {
        boost::system::error_code ec;
        this->socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        this->socket_->close(ec);
        return ec;
    }

private:
    std::vector<std::thread> echo_threads_;
};

class ssl_session: public echo_session<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
{
public:
    using protocol_type = boost::asio::ip::tcp;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    ssl_session(std::shared_ptr<boost::asio::io_service> io_service, std::shared_ptr<boost::asio::ssl::context> context)
        : echo_session<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>()
        , context_(std::move(context))
    {
        io_service_ = std::move(io_service);
        socket_ = std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(*io_service_, *context_);
    }

    ssl_session(const ssl_session& other) = delete;
    ssl_session& operator=(const ssl_session& other) = delete;
    ssl_session(ssl_session&& other) = default;
    ssl_session& operator=(ssl_session&& other) = default;

    virtual ~ssl_session()
    {
        for (auto& t : echo_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void do_echo(const size_t& data_size, const bool& close_after = false)
    {
        echo_threads_.emplace_back([data_size, close_after, this]() {
            size_t recv_bytes = 0;
            boost::system::error_code err_code;
            std::string data(data_size, '\0');

            while (recv_bytes < data_size) {
                recv_bytes += this->socket_->read_some(boost::asio::buffer(&data[recv_bytes], data_size), err_code);
                if (err_code) {
                    if (err_code == boost::asio::error::eof && recv_bytes < data_size) {
                        err_code = boost::asio::error::message_size;
                    }
                    break;
                }
            }

            boost::asio::write(*this->socket_, boost::asio::buffer(&data[0], recv_bytes), err_code);
            if (close_after) {
                this->socket_->shutdown(err_code);
                this->socket_->lowest_layer().close(err_code);
            }
        });
    }

    socket_type::lowest_layer_type& get_socket()
    {
        return socket_->lowest_layer();
    }

    socket_type& get_ssl_socket()
    {
        return *socket_;
    }

    boost::system::error_code close()
    {
        boost::system::error_code ec;
        socket_->lowest_layer().close(ec);
        return ec;
    }

private:
    std::shared_ptr<boost::asio::ssl::context> context_;
    std::vector<std::thread> echo_threads_;
};

class http_session: public echo_session<boost::asio::ip::tcp::socket>
{
public:
    using protocol_type = typename socket_type::protocol_type;
    using endpoint_type = typename socket_type::endpoint_type;

    http_session(std::shared_ptr<boost::asio::io_service> io_service, std::shared_ptr<socket_type> socket)
        : echo_session<boost::asio::ip::tcp::socket>(std::move(io_service), std::move(socket))
    {
    }

    http_session(const http_session& other) = delete;
    http_session& operator=(const http_session& other) = delete;
    http_session(http_session&& other) = default;
    http_session& operator=(http_session&& other) = default;

    virtual ~http_session()
    {
        for (auto& t : echo_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void do_echo(const std::chrono::milliseconds& delay)
    {
        echo_threads_.emplace_back([delay, this]() {
            boost::system::error_code err_code;
            boost::beast::flat_buffer buffer{8192};
            boost::beast::http::request<boost::beast::http::string_body> request;

            boost::beast::http::read(*this->socket_, buffer, request, err_code);
            if (!err_code) {
                boost::beast::http::response<boost::beast::http::string_body> response(boost::beast::http::status::ok,
                                                                                       request.version(),
                                                                                       request.body());
                response.prepare_payload();

                std::this_thread::sleep_for(delay);
                boost::beast::http::write(*this->socket_, response, err_code);
            }
        });
    }

    void do_echo()
    {
        return do_echo(std::chrono::milliseconds(0));
    }

    boost::system::error_code close()
    {
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
        return ec;
    }

private:
    std::vector<std::thread> echo_threads_;
};

class udp_session: public echo_session<boost::asio::ip::udp::socket>
{
public:
    using protocol_type = typename socket_type::protocol_type;
    using endpoint_type = typename socket_type::endpoint_type;

    udp_session(std::shared_ptr<boost::asio::io_service> io_service, std::shared_ptr<socket_type> socket)
        : echo_session<boost::asio::ip::udp::socket>(std::move(io_service), std::move(socket))
    {
    }

    udp_session(const udp_session& other) = delete;
    udp_session& operator=(const udp_session& other) = delete;
    udp_session(udp_session&& other) = default;
    udp_session& operator=(udp_session&& other) = default;

    virtual ~udp_session()
    {
        for (auto& t : echo_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void do_echo(const size_t& data_size, const bool& close_after = false)
    {
        echo_threads_.emplace_back([data_size, close_after, this]() {
            size_t recv_bytes = 0;
            boost::system::error_code err_code;
            std::string data(data_size, '\0');

            while (recv_bytes < data_size) {
                endpoint_type client_endpoint;
                recv_bytes += this->socket_->receive_from(boost::asio::buffer(&data[recv_bytes], data_size),
                                                          client_endpoint, 0, err_code);

                if (err_code) {
                    if (err_code == boost::asio::error::eof && recv_bytes < data_size) {
                        err_code = boost::asio::error::message_size;
                    }
                    break;
                }

                this->socket_->send_to(boost::asio::buffer(&data[0], recv_bytes), client_endpoint, 0, err_code);
            }

            if (close_after) {
                this->close();
            }
        });
    }

    boost::system::error_code close()
    {
        boost::system::error_code ec;
        socket_->close(ec);
        return ec;
    }

private:
    std::vector<std::thread> echo_threads_;
};

template <typename Session>
class echo_server
{
public:
    using session_type = Session;
    using endpoint_type = typename Session::endpoint_type;
    using protocol_type = typename Session::protocol_type;

    echo_server()
        : io_service_(std::make_shared<boost::asio::io_service>())
    {
    }

    echo_server(const echo_server<Session>& other) = delete;
    echo_server<Session>& operator=(const echo_server<Session>& other) = delete;
    echo_server(echo_server<Session>&& other) = delete;
    echo_server<Session>& operator=(echo_server<Session>&& other) = delete;

    virtual ~echo_server() = default;

    virtual std::future<Session> get_session() = 0;

protected:
    std::shared_ptr<boost::asio::io_service> io_service_;
};

template <typename Session, int Backlog>
class tcp_base_server: public echo_server<Session>
{
public:
    using endpoint_type = typename echo_server<Session>::endpoint_type;

    tcp_base_server(const endpoint_type& endpoint)
        : echo_server<Session>()
        , acceptor_(*this->io_service_)
    {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(Backlog);
    }

    virtual ~tcp_base_server()
    {
        for (auto& t : session_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    virtual std::future<Session> get_session() override
    {
        std::promise<Session> promise;
        auto future = promise.get_future();

        session_threads_.emplace_back(
            [this](std::promise<Session>&& promise) {
                std::unique_lock<std::mutex> lk(this->acceptor_mutex_);

                auto sock = std::make_shared<boost::asio::ip::tcp::socket>(*this->io_service_);
                this->acceptor_.accept(*sock);
                promise.set_value(Session(this->io_service_, std::move(sock)));
            },
            std::move(promise));

        return future;
    }

protected:
    std::mutex acceptor_mutex_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> session_threads_;
};

template <int Backlog>
class tcp_server: public tcp_base_server<tcp_session, Backlog>
{
public:
    using client_type = ::stream_client::tcp_client;
    using client_pool_type = ::stream_client::connector::tcp_pool;

    using tcp_base_server<tcp_session, Backlog>::tcp_base_server;
};

template <int Backlog>
class http_server: public tcp_base_server<http_session, Backlog>
{
public:
    using client_type = ::stream_client::http::http_client;
    using client_pool_type = ::stream_client::connector::http_pool;

    using tcp_base_server<http_session, Backlog>::tcp_base_server;
};

template <int Backlog>
class ssl_server: public echo_server<ssl_session>
{
public:
    using client_type = ::stream_client::ssl::ssl_client;
    using client_pool_type = ::stream_client::connector::base_connection_pool<utils::ssl_connector>;

    ssl_server(const endpoint_type& endpoint)
        : echo_server<ssl_session>()
        , acceptor_(*this->io_service_)
        , context_(std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23))
    {
        context_->set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                              boost::asio::ssl::context::single_dh_use);
        context_->use_certificate_chain_file(SSL_USER_CERT);
        context_->use_private_key_file(SSL_USER_KEY, boost::asio::ssl::context::pem);
        context_->use_tmp_dh_file(SSL_DH_PARAMS);

        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(Backlog);
    }

    virtual ~ssl_server()
    {
        for (auto& t : session_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    virtual std::future<ssl_session> get_session() override
    {
        std::promise<ssl_session> promise;
        auto future = promise.get_future();

        session_threads_.emplace_back(
            [this](std::promise<ssl_session>&& promise) {
                std::unique_lock<std::mutex> lk(this->acceptor_mutex_);

                auto session = ssl_session(this->io_service_, context_);
                this->acceptor_.accept(session.get_socket());
                session.get_ssl_socket().handshake(boost::asio::ssl::stream_base::server);
                promise.set_value(std::move(session));
            },
            std::move(promise));

        return future;
    }

private:
    std::mutex acceptor_mutex_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<boost::asio::ssl::context> context_;
    std::vector<std::thread> session_threads_;
};

class udp_server: public echo_server<udp_session>
{
public:
    using client_type = ::stream_client::udp_client;
    using client_pool_type = ::stream_client::connector::udp_pool;

    udp_server(const endpoint_type& endpoint)
        : echo_server<udp_session>()
        , socket_(std::make_shared<boost::asio::ip::udp::socket>(*io_service_))
    {
        socket_->open(endpoint.protocol());
        socket_->set_option(boost::asio::socket_base::reuse_address(true));
        socket_->bind(endpoint);
    }

    virtual ~udp_server()
    {
        for (auto& t : session_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    virtual std::future<udp_session> get_session() override
    {
        std::promise<udp_session> promise;
        auto future = promise.get_future();

        session_threads_.emplace_back(
            [this](std::promise<udp_session>&& promise) {
                promise.set_value(udp_session(this->io_service_, this->socket_));
            },
            std::move(promise));

        return future;
    }

private:
    std::shared_ptr<boost::asio::ip::udp::socket> socket_;
    std::vector<std::thread> session_threads_;
};

} // namespace utils
