#include "fixtures.hpp"
#include "utils/common.hpp"

TYPED_TEST(ResolverEnv, Resolve)
{
    using protocol_type = typename TestFixture::protocol_type;
    using iterator_type = typename TestFixture::iterator_type;
    using endpoint_type = typename iterator_type::value_type::endpoint_type;

    iterator_type dns_results;
    ASSERT_NO_THROW({ dns_results = this->resolver->resolve(); });

    EXPECT_NE(dns_results, iterator_type());
    std::vector<endpoint_type> endpoints;
    while (dns_results != iterator_type()) {
        endpoints.push_back(dns_results->endpoint());
        ++dns_results;
    }

    EXPECT_GE(endpoints.size(), 1);
    EXPECT_LE(endpoints.size(), 2);
    for (const auto& endpoint : endpoints) {
        EXPECT_EQ(endpoint.port(), this->resolve_port);
        if (endpoint.protocol() == protocol_type::v4()) {
            EXPECT_TRUE(endpoint.address() == boost::asio::ip::address_v4::from_string("127.0.0.1") ||
                        endpoint.address() == boost::asio::ip::address_v4::from_string("127.0.1.1"));
        } else if (endpoint.protocol() == protocol_type::v6()) {
            EXPECT_EQ(endpoint.address(), boost::asio::ip::address_v6::from_string("::1"));
        } else {
            FAIL() << "Unexpected protocol";
        }
    }
}

TYPED_TEST(ResolverEnv, ResolveAddr)
{
    this->SetResolver("127.0.0.1");
    ASSERT_NO_THROW(this->resolver->resolve());
}

TYPED_TEST(ResolverEnv, InvalidResolve)
{
    this->SetResolver("invalid_host");
    ASSERT_THROW_ONEOF3(this->resolver->resolve(), boost::asio::error::host_not_found,
                        boost::asio::error::host_not_found_try_again, boost::asio::error::timed_out);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
