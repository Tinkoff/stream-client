#pragma once

namespace stream_client {
namespace connector {

template <typename Connector, typename ReconnectionStrategy>
template <typename... ArgN>
base_connection_pool<Connector, ReconnectionStrategy>::base_connection_pool(std::size_t size, time_duration_type idle_timeout, ArgN&&... argn)
    : connector_(std::forward<ArgN>(argn)...)
    , pool_max_size_(size)
    , idle_timeout_(idle_timeout)
    , watch_pool_(true)
{
    pool_watcher_ = std::thread([this]() { this->watch_pool_routine(); });
}

template <typename Connector, typename ReconnectionStrategy>
template <typename Arg1, typename... ArgN,
          typename std::enable_if<!std::is_convertible<Arg1, typename Connector::time_duration_type>::value>::type*>
base_connection_pool<Connector, ReconnectionStrategy>::base_connection_pool(std::size_t size, Arg1&& arg1, ArgN&&... argn)
    : base_connection_pool(size, time_duration_type::max(), std::forward<Arg1>(arg1), std::forward<ArgN>(argn)...)
{
}

template <typename Connector, typename ReconnectionStrategy>
base_connection_pool<Connector, ReconnectionStrategy>::~base_connection_pool()
{
    watch_pool_.store(false, std::memory_order_release);
    if (pool_watcher_.joinable()) {
        pool_watcher_.join();
    }
}

template <typename Connector, typename ReconnectionStrategy>
std::unique_ptr<typename base_connection_pool<Connector, ReconnectionStrategy>::stream_type>
base_connection_pool<Connector, ReconnectionStrategy>::get_session(boost::system::error_code& ec, const time_point_type& deadline)
{
    std::unique_lock<std::timed_mutex> pool_lk(pool_mutex_, std::defer_lock);
    if (!pool_lk.try_lock_until(deadline)) {
        // failed to lock pool_mutex_
        ec = boost::asio::error::timed_out;
        return nullptr;
    }
    if (sesson_pool_.empty() && !pool_cv_.wait_until(pool_lk, deadline, [this] { return !sesson_pool_.empty(); })) {
        // session pool is still empty
        ec = boost::asio::error::not_found;
        return nullptr;
    }

    std::unique_ptr<stream_type> session = std::move(sesson_pool_.front().second);
    sesson_pool_.pop_front();
    return session;
}

template <typename Connector, typename ReconnectionStrategy>
std::unique_ptr<typename base_connection_pool<Connector, ReconnectionStrategy>::stream_type>
base_connection_pool<Connector, ReconnectionStrategy>::try_get_session(boost::system::error_code& ec, const time_point_type& deadline)
{
    std::unique_lock<std::timed_mutex> pool_lk(pool_mutex_, std::defer_lock);
    if (!pool_lk.try_lock_until(deadline)) {
        // failed to lock pool_mutex_
        ec = boost::asio::error::timed_out;
        return nullptr;
    }
    if (sesson_pool_.empty()) {
        // session pool is empty
        ec = boost::asio::error::not_found;
        return nullptr;
    }

    std::unique_ptr<stream_type> session = std::move(sesson_pool_.front().second);
    sesson_pool_.pop_front();
    return session;
}

template <typename Connector, typename ReconnectionStrategy>
void base_connection_pool<Connector, ReconnectionStrategy>::return_session(std::unique_ptr<stream_type>&& session)
{
    if (!session || !session->next_layer().is_open()) {
        return;
    }

    std::unique_lock<std::timed_mutex> pool_lk(pool_mutex_, std::defer_lock);
    if (!pool_lk.try_lock_for(std::chrono::milliseconds(1))) {
        // if we failed to return session in 1ms it's easier to establish new one
        return;
    }

    sesson_pool_.emplace_back(clock_type::now(), std::move(session));
    pool_lk.unlock();
    pool_cv_.notify_all();
}

template <typename Connector, typename ReconnectionStrategy>
void base_connection_pool<Connector, ReconnectionStrategy>::append_session(std::unique_ptr<stream_type>&& session)
{
    // ensure only single session added at time
    std::unique_lock<std::timed_mutex> pool_lk(pool_mutex_);
    sesson_pool_.emplace_back(clock_type::now(), std::move(session));
    pool_lk.unlock();

    // unblock one waiting thread
    pool_cv_.notify_one();
}

template <typename Connector, typename ReconnectionStrategy>
bool base_connection_pool<Connector, ReconnectionStrategy>::is_connected(boost::system::error_code& ec, const time_point_type& deadline) const
{
    std::unique_lock<std::timed_mutex> pool_lk(pool_mutex_, std::defer_lock);
    if (!pool_lk.try_lock_until(deadline)) {
        // failed to lock pool_mutex_
        ec = boost::asio::error::timed_out;
        return false;
    }
    if (sesson_pool_.empty() && !pool_cv_.wait_until(pool_lk, deadline, [this] { return !sesson_pool_.empty(); })) {
        // session pool is still empty
        return false;
    }
    return true;
}

template <typename Connector>
struct reconnection_strategy_greedy {
    template<typename append_session_func_type>
    bool proceed(Connector& connector, std::size_t vacant_places, append_session_func_type append_func)
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

static const boost::system::system_error noerr_sys = boost::system::system_error(make_error_code(boost::system::errc::success));
template <typename Connector>
struct reconnection_strategy_conservative {
    typedef std::chrono::steady_clock clk;

    reconnection_strategy_conservative()
        : r_generator(r_device_())
    {
    }

    template<typename append_session_func_type>
    bool proceed(Connector& connector, std::size_t vacant_places, append_session_func_type append_func)
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
        backoff_ms_ = std::min(max_ms_, backoff_ms_);
        wait_until_ = clk::now() + std::chrono::milliseconds(backoff_ms_);

        return false;
    }

    static constexpr unsigned long max_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(10)).count();
    clk::time_point wait_until_ = {};
    unsigned long backoff_ms_ = 0;
    std::random_device r_device_;
    std::mt19937 r_generator;
};

template <typename Connector, typename ReconnectionStrategy>
void base_connection_pool<Connector, ReconnectionStrategy>::watch_pool_routine()
{
    static const auto lock_timeout = std::chrono::milliseconds(100);

    while (watch_pool_.load(std::memory_order_acquire)) {
        // try to lock pool mutex
        std::unique_lock<std::timed_mutex> pool_lk(pool_mutex_, std::defer_lock);
        if (!pool_lk.try_lock_for(lock_timeout)) {
            continue;
        }

        // remove session which idling past idle_timeout_
        std::size_t pool_current_size = 0;
        for (auto pool_it = sesson_pool_.begin(); pool_it != sesson_pool_.end();) {
            const auto idle_for = clock_type::now() - pool_it->first;
            if (idle_for >= idle_timeout_) {
                pool_it = sesson_pool_.erase(pool_it);
            } else {
                ++pool_it;
                ++pool_current_size;
            }
        }

        // release pool mutex after removing old sessions
        pool_lk.unlock();

        // pool_current_size may be bigger if someone returned previous session
        std::size_t vacant_places = (pool_max_size_ > pool_current_size) ? pool_max_size_ - pool_current_size : 0;

        if (vacant_places) {
            const auto need_more = reconnection_.proceed(connector_, vacant_places, [this](std::unique_ptr<stream_type>&& session) {
                this->append_session(std::move(session));
            });
            if (need_more) {
                continue;
            }
        }

        // stop cpu spooling if nothing has been added
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace connector
} // namespace stream_client
