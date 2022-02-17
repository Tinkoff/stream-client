#pragma once

#include <functional>
#include <memory>
#include <random>
#include <type_traits>

namespace stream_client {
namespace connector {

/**
 * Interface for pool reconnection strategy. Used by connection_pool to refill itself.
 */
template <typename Connector>
class pool_strategy
{
public:
    using connector_type = typename std::remove_reference<Connector>::type;
    using stream_type = typename connector_type::stream_type;
    using append_func_type = typename std::function<void(std::unique_ptr<stream_type>&& session)>;

    /// Destructor.
    virtual ~pool_strategy() = default;

    /**
     * Creates new sessions via @p connector and add them to the pool up to @p vacant_places times.
     *
     * @param connector Connector to use for new sessions.
     * @param vacant_places Number of required connection to fulfill the pool.
     * @param append_func This function is used to add new one connected session to pool.
     *
     * @returns true if all session have been successfully added.
     */
    virtual bool refill(connector_type& connector, std::size_t vacant_places, append_func_type append_func) = 0;
};

/**
 * Greedy strategy. Will refill pool completely using multiple threads.
 */
template <typename Connector>
class greedy_strategy: public pool_strategy<Connector>
{
public:
    using connector_type = typename pool_strategy<Connector>::connector_type;
    using stream_type = typename pool_strategy<Connector>::stream_type;
    using append_func_type = typename pool_strategy<Connector>::append_func_type;

    greedy_strategy() = default;

    /// Destructor.
    virtual ~greedy_strategy() = default;

    /**
     * Adds up to @p vacant_places sessions via @p connector simultaneously.
     *
     * @param connector Connector to use for new sessions.
     * @param vacant_places Number of required connection to fulfill the pool.
     * @param append_func Function is used to add new session to the pool.
     *
     * @returns true if @p vacant_places > 0.
     */
    virtual bool refill(connector_type& connector, std::size_t vacant_places, append_func_type append_func) override;
};

/**
 * Conservative strategy. Will try to refill up to 2/3 of vacant places in the pool.
 * If failed will back off with delay and try to add only one new session.
 */
template <typename Connector>
class conservative_strategy: public pool_strategy<Connector>
{
public:
    using connector_type = typename pool_strategy<Connector>::connector_type;
    using stream_type = typename pool_strategy<Connector>::stream_type;
    using append_func_type = typename pool_strategy<Connector>::append_func_type;

    using clock_type = typename connector_type::clock_type;
    using time_duration_type = typename connector_type::time_duration_type;
    using time_point_type = typename connector_type::time_point_type;

    /// Maximum allowed delay is ms.
    static const unsigned long kMaxBackoffMs;
    static const unsigned long kDefaultDelayMs;
    static const unsigned long kDefaultDelayMul;

    /**
     * Creates conservative strategy with specified delay and multiplier.
     * Strategy allows to add new sessions with increasing delays upon failures.
     *
     * @param first_delay_ms Initial delay to use.
     * @param delay_multiplier Multiply delay by this number each time we fail. Should be >= 1
     */
    conservative_strategy(unsigned long first_delay_ms = kDefaultDelayMs, unsigned delay_multiplier = kDefaultDelayMul);

    /// Destructor.
    virtual ~conservative_strategy() = default;

    /**
     * Adds up to 2/3 of @p vacant_places sessions via @p connector simultaneously.
     * On failures will set a delay and until it is passed will do nothing.
     * Also, will add only one session if previously failed.
     *
     * Delay initially set to `first_delay_ms` and multiplied by random number in [1..`delay_multiplier`)
     * interval on each fail.
     *
     * @param connector Connector to use for new sessions.
     * @param vacant_places Number of required connection to fulfill the pool.
     * @param append_func Function is used to add new session to the pool.
     *
     * @returns true if added at least one session; false if failed or wait time is not reached.
     */
    virtual bool refill(connector_type& connector, std::size_t vacant_places, append_func_type append_func) override;

private:
    time_point_type wait_until_;
    unsigned long initial_delay_ms_;
    unsigned long delay_multiplier_;
    unsigned long current_delay_ms_;

    std::random_device r_device_;
    std::mt19937 r_generator_;
};

} // namespace connector
} // namespace stream_client

#include "impl/pool_strategy.ipp"
