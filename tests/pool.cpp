#include "fixtures.hpp"
#include "utils/common.hpp"

#include <unordered_set>

template <typename server_session_type, typename client_pool_type, typename protocol_type, typename client_type,
          typename Server>
void start_pool_test(ServerEnv<Server>& env)
{
    const size_t pool_size = 10;
    std::vector<std::future<server_session_type>> future_sessions;
    for (size_t i = 0; i < pool_size; ++i) {
        future_sessions.emplace_back(env.server.get_session());
    }

    std::unique_ptr<client_pool_type> clients_pool;
    ASSERT_NO_THROW({
        clients_pool = std::make_unique<client_pool_type>(pool_size, env.host, std::to_string(env.port),
                                                          std::chrono::seconds(1), std::chrono::seconds(1),
                                                          std::chrono::seconds(1));
    });

    std::vector<server_session_type> server_sessions;
    for (size_t i = 0; i < pool_size; ++i) {
        server_sessions.emplace_back(future_sessions[i].get());
    }
    boost::system::error_code error;
    EXPECT_TRUE(clients_pool->is_connected(error));
    EXPECT_CODE(error, boost::system::errc::success);

    // pool should have pool_size of different opened sessions
    std::unordered_set<client_type*> clients;
    for (size_t i = 0; i < pool_size * 5; ++i) {
        auto client_handle = clients_pool->get_session(error);
        ASSERT_CODE(error, boost::system::errc::success);
        EXPECT_TRUE(client_handle->is_open());

        clients.insert(client_handle.get());
        clients_pool->return_session(std::move(client_handle));
    }

    if (typeid(protocol_type) == typeid(boost::asio::ip::udp)) {
        // udp will have at least one session
        EXPECT_GE(clients.size(), 1);
        EXPECT_LE(clients.size(), pool_size);
    } else {
        // tcp may overshoot or undershoot with one session
        EXPECT_GE(clients.size(), pool_size - 1);
        EXPECT_LE(clients.size(), pool_size + 1);
    }
}

TYPED_TEST(GreedyPoolServerEnv, PoolConnect)
{
    using server_session_type = typename TestFixture::session_type;
    using client_pool_type = typename TestFixture::client_pool_type;
    using protocol_type = typename TestFixture::protocol_type;
    using client_type = typename TestFixture::client_type;
    start_pool_test<server_session_type, client_pool_type, protocol_type, client_type>(*this);
}

TYPED_TEST(ConservativePoolServerEnv, PoolConnect)
{
    using server_session_type = typename TestFixture::session_type;
    using client_pool_type = typename TestFixture::client_pool_type;
    using protocol_type = typename TestFixture::protocol_type;
    using client_type = typename TestFixture::client_type;
    start_pool_test<server_session_type, client_pool_type, protocol_type, client_type>(*this);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
