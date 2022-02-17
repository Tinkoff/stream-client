#include "gtest/gtest.h"

#include <array>
#include <map>

#include "stream-client/logger.hpp"

using namespace stream_client;

class FuncLogger: public testing::TestWithParam<log_level>
{
public:
    virtual void SetUp() {
        set_logger(GetParam(), [this](log_level level, const std::string& location, const std::string& message) {
                this->message(level, location, message);
        });
    }

    virtual void TearDown() {
        messages.clear();
        set_logger(nullptr);
    }

    struct msg_content {
        log_level level;
        std::string location;
        std::string message;
    };
    std::vector<msg_content> messages;
private:
    void message(log_level level, const std::string& location, const std::string& message) {
        auto msg = msg_content{level, location, message};
        messages.emplace_back(std::move(msg));
        assert(messages.back().message == message);
    }
};

class CoutLogger: public testing::TestWithParam<log_level>
{
public:
    virtual void SetUp() {
        auto logger = std::make_shared<stream_client::cout_logger>(GetParam());
        stream_client::set_logger(logger);

        log_stream_ = std::make_unique<std::ostringstream>();
        orig_buf_ = std::cout.rdbuf();
        std::cout.rdbuf(log_stream_->rdbuf());
    }

    virtual void TearDown() {
        std::cout.rdbuf(orig_buf_);
        log_stream_.reset();
        set_logger(nullptr);
    }

    std::unique_ptr<std::ostringstream> log_stream_;
private:
    std::basic_streambuf<char, std::char_traits<char>>* orig_buf_;
};

TEST_P(FuncLogger, SetGetLevels)
{
    const auto level = GetParam();
    set_log_level(level);
    EXPECT_EQ(level, get_log_level());
}

TEST_P(FuncLogger, LogMessageCheck)
{
    std::map<log_level, std::string> message_map({
        {log_level::trace, "trace it"},
        {log_level::debug, "this is a debug message"},
        {log_level::info, "application started"},
        {log_level::warning, "bad happens"},
        {log_level::error, "invalid arguments"},
    });

    const auto level = GetParam();
    STREAM_LOG_ERROR(message_map[log_level::error]);
    STREAM_LOG_WARN(message_map[log_level::warning]);
    STREAM_LOG_INFO(message_map[log_level::info]);
    STREAM_LOG_DEBUG(message_map[log_level::debug]);
    STREAM_LOG_TRACE(message_map[log_level::trace]);

    switch (level) {
        case log_level::trace:
            EXPECT_EQ(5, messages.size());
            break;
        case log_level::debug:
            EXPECT_EQ(4, messages.size());
            break;
        case log_level::info:
            EXPECT_EQ(3, messages.size());
            break;
        case log_level::warning:
            EXPECT_EQ(2, messages.size());
            break;
        case log_level::error:
            EXPECT_EQ(1, messages.size());
            break;
        case log_level::mute:
            break;
    }
    switch (level) {
        case log_level::trace:
            EXPECT_EQ("logger.cpp:83", messages[4].location);
            EXPECT_EQ(message_map[log_level::trace], messages[4].message);
            EXPECT_EQ(log_level::trace, messages[4].level);
            [[fallthrough]];
        case log_level::debug:
            EXPECT_EQ("logger.cpp:82", messages[3].location);
            EXPECT_EQ(message_map[log_level::debug], messages[3].message);
            EXPECT_EQ(log_level::debug, messages[3].level);
            [[fallthrough]];
        case log_level::info:
            EXPECT_EQ("logger.cpp:81", messages[2].location);
            EXPECT_EQ(message_map[log_level::info], messages[2].message);
            EXPECT_EQ(log_level::info, messages[2].level);
            [[fallthrough]];
        case log_level::warning:
            EXPECT_EQ("logger.cpp:80", messages[1].location);
            EXPECT_EQ(message_map[log_level::warning], messages[1].message);
            EXPECT_EQ(log_level::warning, messages[1].level);
            [[fallthrough]];
        case log_level::error:
            EXPECT_EQ("logger.cpp:79", messages[0].location);
            EXPECT_EQ(message_map[log_level::error], messages[0].message);
            EXPECT_EQ(log_level::error, messages[0].level);
            break;
        case log_level::mute:
            break;
    }
}

TEST_P(CoutLogger, LogMessageCheck)
{
    std::map<log_level, std::string> message_map({
        {log_level::trace, "trace it"},
        {log_level::debug, "this is a debug message"},
        {log_level::info, "application started"},
        {log_level::warning, "bad happens"},
        {log_level::error, "invalid arguments"},
    });

    const auto level = GetParam();
    set_log_level(level);
    STREAM_LOG_TRACE(message_map[log_level::trace]);
    STREAM_LOG_DEBUG(message_map[log_level::debug]);
    STREAM_LOG_INFO(message_map[log_level::info]);
    STREAM_LOG_WARN(message_map[log_level::warning]);
    STREAM_LOG_ERROR(message_map[log_level::error]);

    std::string line;
    std::istringstream in(log_stream_->str());
    switch (level) {
        case log_level::trace:
            std::getline(in, line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "TRACE", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "logger.cpp:", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, message_map[log_level::trace], line);
            [[fallthrough]];
        case log_level::debug:
            std::getline(in, line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "DEBUG", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "logger.cpp:", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, message_map[log_level::debug], line);
            [[fallthrough]];
        case log_level::info:
            std::getline(in, line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "INFO", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "logger.cpp:", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, message_map[log_level::info], line);
            [[fallthrough]];
        case log_level::warning:
            std::getline(in, line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "WARNING", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "logger.cpp:", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, message_map[log_level::warning], line);
            [[fallthrough]];
        case log_level::error:
            std::getline(in, line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "ERROR", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, "logger.cpp:", line);
            EXPECT_PRED_FORMAT2(testing::IsSubstring, message_map[log_level::error], line);
            [[fallthrough]];
        case log_level::mute:
            break;
    }
}

static const auto AllLogLevels = ::testing::Values(log_level::trace, log_level::debug, log_level::info,
        log_level::warning, log_level::error);
INSTANTIATE_TEST_SUITE_P(SetGetLevels, FuncLogger, AllLogLevels);
INSTANTIATE_TEST_SUITE_P(LogMessageCheck, FuncLogger, AllLogLevels);

INSTANTIATE_TEST_SUITE_P(SetGetLevels, CoutLogger, AllLogLevels);
INSTANTIATE_TEST_SUITE_P(LogMessageCheck, CoutLogger, AllLogLevels);

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
