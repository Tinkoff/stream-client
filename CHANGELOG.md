## 1.2.0 (2021-11-24)

### Features

* http/base_socket: Use dynamic allocator for transfered data


## 1.1.14 (2021-11-23)

### Bug Fixes

* connection_pool: Fix wrong error message "Too many open descriptors"


## 1.1.13 (2021-11-17)

### Bug Fixes

* connector: Fix data race for random generator
* tests/timeout: Remove time check for zero timeout

### Misc

* change googletest GIT_TAG to main
* workflow: Explicitly install openssl with brew
* readme: Add  n.suboch to maintainers list


## 1.1.12 (2021-09-21)

### Bug Fixes

* connector::base_connector: fix resolving thread spooling if attemts failed

### Misc

* Fix github 'lint' action on PRs


## 1.1.11 (2021-08-20)

### Bug Fixes

* connector::base_connector: fix UB in dealing with boolean flag

### Misc

* Add thread/address/UB sanitizers build to CI


## 1.1.10 (2021-08-17)

### Bug Fixes

* connector::base_connection_pool: fix UB in dealing with boolean flag

### Misc

* Add github actions
* Update README
* Lint the code


## 1.1.9 (2021-07-30)

### Features

* connector::base_connection_pool: add try_get_session method
* connector::base_connection_pool: change global lock in watch_pool_routine with several small locks 


## 1.1.8 (2021-07-14)

### Features

* ssl::stream_socket: set SNI hostname in ctor


## 1.1.7 (2021-06-30)

### Bug Fixes

* Fix mem leakage in static_allocator used for HTTP requests


## 1.1.6 (2021-06-29)

### Bug Fixes

* Fix static const undefined reference on some compilers for kHeaderLimit/kBodyLimit


## 1.1.5 (2021-02-12)

### Features

* Add idle timeout parameter for a pool. If session is sitting idle in the pool for a longer time it will replaced with new one


## 1.1.4 (2020-04-28)

### Bug Fixes

* TCP options not set in base_socket ctor


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
