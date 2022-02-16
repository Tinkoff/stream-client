#pragma once

#include <boost/system/system_error.hpp>

#include <atomic>
#include <chrono>
#include <list>
#include <random>
#include <thread>
#include <vector>

namespace stream_client {
namespace connector {

template <typename Connector>
const unsigned long conservative_strategy<Connector>::kMaxBackoffMs = 10000; // 10 seconds maximum delay

template <typename Connector>
const unsigned long conservative_strategy<Connector>::kDefaultDelayMs = 50; // 50 milliseconds is default initial delay

template <typename Connector>
const unsigned long conservative_strategy<Connector>::kDefaultDelayMul = 3; // 3 is default delay multiplier

template <typename Connector>
bool greedy_strategy<Connector>::refill(connector_type& connector, std::size_t vacant_places,
                                        append_func_type append_func)
{
    // creating new sessions may be slow and we want to add them simultaneously
    auto add_session = [&]() {
        try {
            auto new_session = connector.new_session();
            append_func(std::move(new_session));
        } catch (const boost::system::system_error& e) {
            // TODO: log errors ?
        }
    };

    std::list<std::thread> adders;
    for (std::size_t i = 0; i < vacant_places; ++i) {
        adders.emplace_back(add_session);
    }
    for (auto& a : adders) {
        a.join();
    }

    return vacant_places > 0;
}

template <typename Connector>
conservative_strategy<Connector>::conservative_strategy(unsigned long first_delay_ms, unsigned delay_multiplier)
    : initial_delay_ms_(first_delay_ms)
    , delay_multiplier_(delay_multiplier)
    , current_delay_ms_(0)
    , r_generator_(r_device_())
{
    if (delay_multiplier_ < 1) {
        throw std::runtime_error("delay multiplier should be >= 1");
    }
}

template <typename Connector>
bool conservative_strategy<Connector>::refill(connector_type& connector, std::size_t vacant_places,
                                              append_func_type append_func)
{
    if (clock_type::now() < wait_until_) {
        return false;
    }

    std::atomic_bool is_added{false};

    // creating new sessions may be slow and we want to add them simultaneously
    auto add_session = [&]() {
        try {
            auto new_session = connector.new_session();
            append_func(std::move(new_session));
            is_added = true;
        } catch (const boost::system::system_error& e) {
            // TODO: log errors ?
        }
    };

    std::vector<std::thread> adders;
    const size_t parallel = (vacant_places + 2) / 3 - 1;
    if (!current_delay_ms_ && parallel > 0) {
        adders.reserve(parallel);
        for (std::size_t i = 0; i < parallel; ++i) {
            adders.emplace_back(add_session);
        }
    }
    add_session();
    for (auto& a : adders) {
        a.join();
    }

    if (is_added) {
        current_delay_ms_ = 0;
        return true;
    }

    if (!current_delay_ms_) {
        current_delay_ms_ = initial_delay_ms_;
    } else {
        current_delay_ms_ *= delay_multiplier_;
    }
    const auto rand_val = double(r_generator_()) / r_generator_.max();
    current_delay_ms_ *= rand_val;
    current_delay_ms_ = std::min(kMaxBackoffMs, current_delay_ms_);
    wait_until_ = clock_type::now() + std::chrono::milliseconds(current_delay_ms_);

    return false;
}

} // namespace connector
} // namespace stream_client
