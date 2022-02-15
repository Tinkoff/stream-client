#pragma once

#include <boost/system/error_code.hpp>

#include <chrono>
#include <list>
#include <random>
#include <vector>

namespace stream_client {
namespace connector {

template <typename Connector>
struct reconnection_strategy_greedy {
    template<typename AppendSessionFunc>
    bool proceed(Connector& connector, std::size_t vacant_places, AppendSessionFunc append_func)
    {
        // creating new sessions may be slow and we want to add them simultaneously;
        // that's why we need to sync adding threads and lock pool
        auto add_session = [&]() {
            try {
                // getting new session is time consuming operation
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
};

static constexpr unsigned long MAX_BACKOFF_MS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(10)).count();

static const boost::system::system_error noerr_sys = boost::system::system_error(make_error_code(boost::system::errc::success));
template <typename Connector>
struct reconnection_strategy_conservative {
    typedef std::chrono::steady_clock clk;

    reconnection_strategy_conservative()
        : r_generator(r_device_())
    {
    }

    template<typename AppendSessionFunc>
    bool proceed(Connector& connector, std::size_t vacant_places, AppendSessionFunc append_func)
    {
        if (clk::now() < wait_until_) {
            return false;
        }

        std::atomic_bool is_added{false};

        // creating new sessions may be slow and we want to add them simultaneously;
        // that's why we need to sync adding threads and lock pool
        auto add_session = [&]() {
            try {
                // getting new session is time consuming operation
                auto new_session = connector.new_session();
                append_func(std::move(new_session));
                is_added = true;
            } catch (const boost::system::system_error& e) {
                // TODO: log errors ?
            }
        };

        std::vector<std::thread> adders;
        const size_t parallel = (vacant_places + 2) / 3 - 1;
        if (!backoff_ms_ && parallel > 0) {
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
            backoff_ms_ = 0;
            return true;
        }

        if (!backoff_ms_) {
            backoff_ms_ = 50;
        } else {
            backoff_ms_ *= 3;
        }
        const auto rand_val = double(r_generator()) / r_generator.max();
        backoff_ms_ *= rand_val;
        backoff_ms_ = std::min(MAX_BACKOFF_MS, backoff_ms_);
        wait_until_ = clk::now() + std::chrono::milliseconds(backoff_ms_);

        return false;
    }

    clk::time_point wait_until_ = {};
    unsigned long backoff_ms_ = 0;
    std::random_device r_device_;
    std::mt19937 r_generator;
};

} // namespace connector
} // namespace stream_client
