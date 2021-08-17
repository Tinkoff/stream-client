#include "fixtures.hpp"
#include "utils/common.hpp"

#include <unordered_set>

TYPED_TEST(PoolServerEnv, PoolConnect)
{
    using server_session_type = typename TestFixture::session_type;
    using client_pool_type = typename TestFixture::client_pool_type;
    using client_type = typename TestFixture::client_type;

    const size_t pool_size = 1;
    std::vector<std::future<server_session_type>> future_sessions;
    for (size_t i = 0; i < pool_size; ++i) {
        future_sessions.emplace_back(this->server.get_session());
    }

    std::unique_ptr<client_pool_type> clients_pool;
    ASSERT_NO_THROW({
        clients_pool = std::make_unique<client_pool_type>(pool_size, this->host, std::to_string(this->port),
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

    // pool should have a pool_size of different opened sessions
    std::unordered_set<client_type*> clients;
    for (size_t i = 0; i < pool_size; ++i) {
        boost::system::error_code error;
        auto client_handle = clients_pool->get_session(error);
        ASSERT_CODE(error, boost::system::errc::success);
        EXPECT_TRUE(client_handle->is_open());

        clients.insert(client_handle.get());
        clients_pool->return_session(std::move(client_handle));
    }
    EXPECT_EQ(clients.size(), pool_size);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
