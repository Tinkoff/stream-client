# Stream-client

[![Language C++](https://img.shields.io/badge/language-c++-blue.svg?logo=c%2B%2B)](https://isocpp.org)
[![Github releases](https://img.shields.io/github/release/TinkoffCreditSystems/stream-client.svg)](https://github.com/TinkoffCreditSystems/stream-client/releases)
[![Coverage Status](https://coveralls.io/repos/github/TinkoffCreditSystems/stream-client/badge.svg?branch=develop)](https://coveralls.io/github/TinkoffCreditSystems/stream-client?branch=develop)
[![License](https://img.shields.io/github/license/TinkoffCreditSystems/stream-client.svg)](./LICENSE)

This is a lightweight, header-only, Boost-based library providing client-side network primitives to easily organize and implement data transmission with remote endpoints.

This library:
* Inspired by and built around [Boost.Asio](http://boost.org/libs/asio/).
* Provides high-level constructs as [connector](#connector) and [connection pool](#connection-pool).
* Supports **TCP**/**UDP**/**SSL**/**HTTP**/**HTTPS** protocols.
* Uses [sockets](#sockets) with the same interface as boost::asio::ip ones.
* Allows transparent timeout/deadline control for all operations.
* Requires boost 1.65+ or 1.66+ if you are planning to use HTTP protocol.

## Why

If you are writing software on C++, which communicates with other services as a client, you probably already came-across with the problem - need to implement connectivity layer to provide network transport to all related services. This is exactly what this library's aim to solve.

## How to use

### Sockets

Sockets are implemented on top of `boost::asio::basic_socket` and provide classes with timeout control, so in most cases it's enough to just call *send()*/*receive()*/*write_some()*/*read_some()* with a deadline or timeout. Data supplied to I/O operations should be wrapped into `boost::asio::buffer`. Basically these clients are timeout-wrapped `boost::asio::ip` sockets and have the same interface.

Client streams classes:
* `stream_client::tcp_client` - plain TCP stream socket client. Supports *send()*/*rececive()* for admitted transfers of whole buffer along with *write_some()*/*read_some()* to transfer at least something.
* `stream_client::udp_client` - plain UDP socket client. Supports only *send()*/*receive()* without any acknowledgment as per UDP specs.
* `stream_client::ssl::ssl_client` - SSL-encrypted TCP client. Have the same functions as `tcp_client` plus SSL handshake and context control.
* `stream_client::http::http_client` - HTTP client. Wraps `tcp_client` with `boost::beast::http::parser` and `boost::beast::http::serializer` and have *perform()* function to make request-response calls.
* `stream_client::http::https_client` - HTTPS client. Same as `http_client` but uses `ssl_client` client underneath.

#### Example
```c++
const boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 12345);
const std::chrono::milliseconds connect_timeout(1000);
const std::chrono::milliseconds io_timeout(100);

// this makes connected socket and may take up to connect_timeout time
stream_client::tcp_client client(endpoint, connect_timeout, io_timeout);

// send() operation will transmit all contents of send_data or throw an error;
// also it will throw boost::asio::error::timed_out after io_timeout.
const std::string send_data = "test data";
client.send(boost::asio::buffer(send_data.data(), send_data.size()));

// receive() operation will read send_data.size() size or throw an error;
// also it will throw boost::asio::error::timed_out after io_timeout.
std::string recv_data(send_data.size(), '\0');
client.receive(boost::asio::buffer(&recv_data[0], send_data.size()));

// there are also non-throw overloads to these functions which
// will set boost::system::error_code instead of exception;
```

### DNS resolver

Resolver is implemented around the same idea - make sync, timed DNS resolver, therefore it uses `boost::asio::ip::basic_resolver` to perform actual resolution within specified timeouts.

It is the class template with a single parameter - result protocol, also you can specify which type of IP endpoints you are interested in - IPv4, IPv6 or both. Resolution performed with *resolve()* call which has a similar signature as sockets operations - you are free to specify timeout/deadline or use default one.

Resolver classes:
* `stream_client::resolver::tcp_resolver` - resolver which returns iterator of TCP endpoints.
* `stream_client::resolver::udp_resolver` - resolver which returns iterator of UDP endpoints.

#### Example
```c++
const std::chrono::milliseconds resolve_timeout(5000);

stream_client::resolver::tcp_resolver resolver("localhost", "12345",
                                               resolve_timeout,
                                               stream_client::resolver::ip_family::ipv4);

// resolve() guaranteed to return at least one endpoint or will throw an error;
// it will throw boost::asio::error::timed_out after resolve_timeout.
auto endpoints_it = resolver.resolve();

// returned iterator is an instance of boost::asio::ip::basic_resolver::iterator<Protocol>
```

### Connector

Connector uses [resolver](#dns-resolver) to perform DNS resolution and update the list of endpoints and uses them if requested with *new_session()*, to create a new socket of the specified protocol. The work of DNS resolver is wrapped into background thread which triggered upon creation or if there was an error in opening a new socket.

Target endpoint to open a new socket selected randomly from the DNS results, which makes it **balanced connector** to a remote host. Also, the connector allows specifying separate timeouts for resolution, connection, and I/O operation of new sockets.

Connector classes:
* `stream_client::connector::tcp_connector` - TCP connector, to obtain `stream_client::tcp_client` sockets.
* `stream_client::connector::udp_connector` - UDP connector, to obtain `stream_client::udp_client` sockets.
* `stream_client::connector::ssl_connector` - TCP connector, to obtain `stream_client::ssl::ssl_client` sockets.
* `stream_client::connector::http_connector` - HTTP connector, to obtain `stream_client::http::http_client` sockets.
* `stream_client::connector::https_connector` - HTTPS connector, to obtain `stream_client::http::https_client` sockets.

#### Example
```c++
const std::chrono::milliseconds resolve_timeout(5000);
const std::chrono::milliseconds connect_timeout(1000);
const std::chrono::milliseconds io_timeout(100);

// this will return immediately, starting background thread for name resolution
stream_client::connector::tcp_connector connector("localhost", "12345",
                                                  resolve_timeout, connect_timeout, io_timeout,
                                                  stream_client::resolver::ip_family::ipv4);

// this will acquire new socket or throw;
// it will throw boost::asio::error::timed_out after connect_timeout
std::unique_ptr<stream_client::tcp_client> client = connector.new_session();

// use the client
const std::string send_data = "test data";
std::string recv_data(send_data.size(), '\0');
client->send(boost::asio::buffer(send_data.data(), send_data.size()));
client->receive(boost::asio::buffer(&recv_data[0], send_data.size()));
```

### Connection pool

Represents container occupied with opened sockets. Uses [connector](#connector) to open new sockets in the background thread which is triggered once there are vacant places in the pool. User can call *get_session()* to obtain a socket from the pool and *return_session()* to give it back.

Limitations:
1. Sockets that are already in the pool are not checked or maintained in any way. Hence, the pool doesn't guarantee that all sockets are opened at an arbitrary point in time due to the complexity of such checks for all supported protocols.
2. Nothing specific done with sockets upon their return within *return_session()*. Therefore, if they have or will have pending data to read, it will stay there until reading.

Considering this, the best strategy to use a connection pool is such:
1. Create it, specifying all timeouts as you need.
2. Once created, use *get_session()* to obtain opened socket.
3. Do needed I/O operations on the socket.
4. If 2-3 succeed, return it back with *return_session()*, else repeat point 2.

*It is important to discard sockets on failure and not reuse them, or request-response management will get nasty.*

Connection pools:
* `stream_client::connector::tcp_pool` - pool of `stream_client::tcp_client` sockets.
* `stream_client::connector::udp_pool` - pool of `stream_client::udp_client` sockets.
* `stream_client::connector::ssl_pool` - pool of `stream_client::ssl::ssl_client` sockets.
* `stream_client::connector::http_pool` - pool of `stream_client::http::http_client` sockets.
* `stream_client::connector::https_pool` - pool of `stream_client::http::https_client` sockets.

#### Example
```c++
const std::chrono::milliseconds resolve_timeout(5000);
const std::chrono::milliseconds connect_timeout(1000);
const std::chrono::milliseconds io_timeout(100);

auto connector_pool =
    std::make_shared<stream_client::connector::tcp_pool>("localhost", "12345",
                                                         resolve_timeout, connect_timeout, io_timeout,
                                                         stream_client::resolver::ip_family::ipv4);
const size_t threads_num = 10;
std::vector<std::thread> threads;
const std::string send_data = "test data";
std::mutex stdout_mutex;

for (size_t i = 0; i < threads_num; ++i) {
    threads.emplace_back([&send_data, &stdout_mutex, connector_pool]() {
        std::string recv_data(send_data.size(), '\0');

        auto client = connector_pool->get_session();
        // both these calls will throw exception on error or timeout
        client->send(boost::asio::buffer(send_data.data(), send_data.size()));
        client->receive(boost::asio::buffer(&recv_data[0], send_data.size()));
        // if there was no exception, return it
        connector_pool->return_session(std::move(client));

        const std::lock_guard<std::mutex> lk(stdout_mutex);
        std::cout << std::this_thread::get_id() << ": " << recv_data << std::endl;
    });
}

for (auto& t : threads) {
    t.join();
}
```

## How to build

This library supposed to be somewhat multi-platform, however, it was tested and mainly used on ubuntu and macOS. </br>
Prefer [out-of-source](https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#what-is-an-out-of-source-build) building.

### Ubuntu dependencies

```bash
sudo apt update
sudo apt install build-essential cmake libboost-dev libboost-system-dev libssl-dev
```

### macOS dependencies

```bash
brew install cmake pkg-config icu4c openssl boost
```

To build:
```bash
cmake -H. -Bbuild
cmake --build ./build
```

To install (sudo may be required):
```bash
cmake -H. -Bbuild -DSTREAMCLIENT_BUILD_TESTING=OFF -DSTREAMCLIENT_BUILD_DOCS=OFF -DSTREAMCLIENT_BUILD_EXAMPLES=OFF
cmake --build ./build --target install
```

Or test:
```bash
cmake -H. -Bbuild -DSTREAMCLIENT_BUILD_TESTING=ON -DSTREAMCLIENT_BUILD_DOCS=OFF -DSTREAMCLIENT_BUILD_EXAMPLES=OFF
cmake --build ./build
cmake -E chdir ./build ctest --output-on-failure
```

*All these commands assume you are in stream-client root folder*

### Cmake options

* **CMAKE_BUILD_TYPE** - [build type](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html). `RelWithDebInfo` by default.
* **BUILD_SHARED_LIBS** - [build shared or static library](https://cmake.org/cmake/help/v3.0/variable/BUILD_SHARED_LIBS.html). `ON` by default.
* **STREAMCLIENT_BUILD_TESTING** - [build tests or not](https://cmake.org/cmake/help/latest/module/CTest.html). `OFF` by default
* **STREAMCLIENT_BUILD_EXAMPLES** - build library examples or not. `OFF` by default.
* **STREAMCLIENT_BUILD_DOCS** – build html (sphinx) reference docs. `OFF` by default.
* **OPENSSL_USE_STATIC_LIBS** - link statically or dynamically against found openssl. If *BUILD_SHARED_LIBS* is `OFF` then this options is set.
* **OPENSSL_ROOT_DIR** - folder where to look for openssl. Set by pkg-config of brew by default.

## License

Developed at **Tinkoff.ru** in 2020.\
Distibuted under **Apache License 2.0** [LICENSE](./LICENSE). You may also obtain this license at https://www.apache.org/licenses/LICENSE-2.0.

## Contacts

Author - i.s.vovk@tinkoff.ru\
Current maintainers - i.s.vovk@tinkoff.ru, n.suboch@tinkoff.ru
