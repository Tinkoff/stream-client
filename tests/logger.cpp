#include "stream-client/logger.hpp"

int main()
{
    const auto used_level = stream_client::log_level::debug;
    auto logger = std::make_shared<stream_client::cout_logger>(used_level);
    stream_client::set_logger(used_level, [logger](stream_client::log_level level, const std::string& location, const std::string& message) {
        logger->message(level, location, message);
    });
    STREAM_LOG_TRACE("just a trace");
    STREAM_LOG_ERROR("funny error");
    STREAM_LOG_DEBUG("debug message from bunny");
    return 0;
}
