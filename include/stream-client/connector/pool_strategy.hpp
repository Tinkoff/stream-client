#pragma once

#include <cstddef>

namespace stream_client {
namespace connector {

/**
 * Template class for reconnection strategy
 *
 * @tparam Connector Type of connector to use to create sockets.
 */
template <typename Connector>
struct reconnection_strategy
{
    /**
     * Main process function for connect new vacant_places sessions.
     *
     * @param connector Instance of connector that allow create new session.
     * @param vacant_places Number of required connection to fullfil the pool. Must be greather than 0.
     * @param append_func This function is used to add new one connected session to pool.
     *
     * @tparam AppendSessionFunc Callback function to add new session to session pool.
     */
    template <typename AppendSessionFunc>
    bool proceed(Connector& connector, std::size_t vacant_places, AppendSessionFunc append_func);
};

template <typename Connector>
struct reconnection_strategy_greedy;
template <typename Connector>
struct reconnection_strategy_conservative;

} // namespace connector
} // namespace stream_client

#include "impl/pool_strategy.ipp"
