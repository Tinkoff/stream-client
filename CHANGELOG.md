## 1.1.3 (2020-03-03)

### Bug Fixes

* get_session(timeout_or_deadline) return invalid socket

## 1.1.2 (2020-03-02)

### Features

* Add throwing get_session(timeout_or_deadline) function to the pool

## 1.1.1 (2020-03-02)

### Features

* Require standard openssl on macos (1.1+)
* Add Apache 2.0 LICENSE file

### Misc

* Small README fixes
* Style-related fixes

# 1.1.0 (2020-01-27)

### Features

* Apply clang-format as code style guard
* Add doxygen-format docs
* Add IPv4/IPV6/Any selector to stream_client::resolver::base_resolver<>
* Allow boost::asio::ip::basic_resolver_query::flags pass-through
* Separate TCP/UDP implementation since they are based on boost::asio::basic_stream_socket/basic_datagram_socket
* Add tests
* Add readme

### Bug Fixes

* Remove redundant ssl-handshake type parameter. Use client one

# 1.0.0 (2019-12-11)

- initial release
