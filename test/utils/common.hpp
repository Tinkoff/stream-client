#pragma once

#include <boost/system/error_code.hpp>
#include <sstream>
#include <string>

#define ASSERT_THROW_CODE(expression, expected_ec)                                                 \
    {                                                                                              \
        const auto expected_err = make_ec(expected_ec);                                            \
        try {                                                                                      \
            (expression);                                                                          \
            FAIL() << "Expected: " + expected_err.message() << "\n  Actual: it throws nothing.";   \
        } catch (const boost::system::system_error& ex) {                                          \
            ASSERT_EQ(ex.code(), expected_err)                                                     \
                << "Expected: " + ec_string(expected_err) << "\n  Actual: " + ex.code().message(); \
        } catch (const std::exception& ex) {                                                       \
            FAIL() << "Expected: " + ec_string(expected_err) << "\n  Actual: " << ex.what();       \
        }                                                                                          \
    }

#define ASSERT_THROW_ONEOF2(expression, expected_ec1, expected_ec2)                                       \
    {                                                                                                     \
        const auto expected_err1 = make_ec(expected_ec1);                                                 \
        const auto expected_err2 = make_ec(expected_ec2);                                                 \
        try {                                                                                             \
            (expression);                                                                                 \
            FAIL() << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) \
                   << "\n         Actual: it throws nothing.";                                            \
        } catch (const boost::system::system_error& ex) {                                                 \
            ASSERT_TRUE(ex.code() == expected_err1 || ex.code() == expected_err2)                         \
                << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2)    \
                << "\n         Actual: " + ec_string(ex.code());                                          \
        } catch (const std::exception& ex) {                                                              \
            FAIL() << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) \
                   << "\n         Actual: " << ex.what();                                                 \
        }                                                                                                 \
    }

#define ASSERT_THROW_ONEOF3(expression, expected_ec1, expected_ec2, expected_ec3)                                 \
    {                                                                                                             \
        const auto expected_err1 = make_ec(expected_ec1);                                                         \
        const auto expected_err2 = make_ec(expected_ec2);                                                         \
        const auto expected_err3 = make_ec(expected_ec3);                                                         \
        try {                                                                                                     \
            (expression);                                                                                         \
            FAIL() << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) << "; " \
                   << ec_string(expected_err3) << "\n         Actual: it throws nothing.";                        \
        } catch (const boost::system::system_error& ex) {                                                         \
            ASSERT_TRUE(ex.code() == expected_err1 || ex.code() == expected_err2 || ex.code() == expected_err3)   \
                << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) << "; "    \
                << ec_string(expected_err3) << "\n         Actual: " + ex.code().message();                       \
        } catch (const std::exception& ex) {                                                                      \
            FAIL() << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) << "; " \
                   << ec_string(expected_err3) << "\n         Actual: " << ex.what();                             \
        }                                                                                                         \
    }

#define ASSERT_CODE(expression, expected_ec)            \
    {                                                   \
        const auto expected_err = make_ec(expected_ec); \
        ASSERT_EQ(expression, expected_err);            \
    }

#define EXPECT_CODE(expression, expected_ec)            \
    {                                                   \
        const auto expected_err = make_ec(expected_ec); \
        EXPECT_EQ(expression, expected_err);            \
    }

#define EXPECT_CODE_ONEOF2(expression, expected_ec1, expected_ec2)                                 \
    {                                                                                              \
        const auto expected_err1 = make_ec(expected_ec1);                                          \
        const auto expected_err2 = make_ec(expected_ec2);                                          \
        const auto actual_ec = expression;                                                         \
        EXPECT_TRUE(actual_ec == expected_err1 || actual_ec == expected_err2)                      \
            << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) \
            << "\n         Actual: " << ec_string(actual_ec);                                      \
    }

#define EXPECT_CODE_ONEOF3(expression, expected_ec1, expected_ec2, expected_ec3)                            \
    {                                                                                                       \
        const auto expected_err1 = make_ec(expected_ec1);                                                   \
        const auto expected_err2 = make_ec(expected_ec2);                                                   \
        const auto expected_err3 = make_ec(expected_ec3);                                                   \
        const auto actual_ec = expression;                                                                  \
        EXPECT_TRUE(actual_ec == expected_err1 || actual_ec == expected_err2 || actual_ec == expected_err3) \
            << "Expected one of: " << ec_string(expected_err1) << "; " << ec_string(expected_err2) << "; "  \
            << ec_string(expected_err3) << "\n         Actual: " << ec_string(actual_ec);                   \
    }

#define EXPECT_TIME_GE(elapsed, expected_timeout)                                             \
    EXPECT_TRUE(elapsed >= expected_timeout)                                                  \
        << "Expected time >= " << std::chrono::microseconds(expected_timeout).count() << "us" \
        << "\n  Actual time = " << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << "us";

#define EXPECT_TIME_LE(elapsed, expected_timeout)                                             \
    EXPECT_TRUE(elapsed <= expected_timeout)                                                  \
        << "Expected time <= " << std::chrono::microseconds(expected_timeout).count() << "us" \
        << "\n  Actual time = " << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << "us";

#define EXPECT_TIMEOUT_GE(now, start_time, expected_timeout) EXPECT_TIME_GE(now - start_time, expected_timeout)
#define EXPECT_TIMEOUT_LE(now, start_time, expected_timeout) EXPECT_TIME_LE(now - start_time, expected_timeout)

#define EXPECT_TIMEOUT(now, start_time, expected_timeout, tolerance) \
    {                                                                \
        const auto elapsed = now - start_time;                       \
        const auto min = expected_timeout - tolerance;               \
        const auto max = expected_timeout + tolerance;               \
        EXPECT_TIME_GE(elapsed, min);                                \
        EXPECT_TIME_LE(elapsed, max);                                \
    }

#define EXPECT_EQ_DURATION(actual, expected) EXPECT_EQ(actual - expected, std::chrono::nanoseconds(0))

class ssl_error: public boost::system::error_code
{
public:
    using boost::system::error_code::error_code;

    ssl_error(int ssl_error_reason)
        : ssl_error(ssl_error_reason, boost::asio::error::get_ssl_category())
    {
    }

    inline friend bool operator==(const ssl_error& lhs, const boost::system::error_code& rhs) noexcept
    {
        return lhs.category() == rhs.category() && lhs.value() == ERR_GET_REASON(rhs.value());
    }

    inline friend bool operator==(const boost::system::error_code& lhs, const ssl_error& rhs) noexcept
    {
        return rhs.category() == lhs.category() && rhs.value() == ERR_GET_REASON(lhs.value());
    }
};

template <typename Error>
auto make_ec(Error err)
{
    boost::system::error_code ec;
    if (err != 0) {
        ec = make_error_code(err);
    }
    if (ec.category() == boost::system::generic_category()) {
        ec.assign(ec.value(), boost::system::system_category());
    }
    return ec;
}

template <>
auto make_ec(ssl_error err)
{
    return err;
}

inline std::string ec_string(const boost::system::error_code& ec)
{
    std::ostringstream ss;
    ss << "[" << ec << "]"
       << " " << ec.message();
    return ss.str();
}

#if BOOST_VERSION >= 106200
auto ssl_short_read_err = boost::asio::ssl::error::stream_truncated;
#else // older Boost supports only OpenSSL 1.0, so 1.0-only macro is appropriate
auto ssl_short_read_err = ssl_error(SSL_R_SHORT_READ);
#endif
