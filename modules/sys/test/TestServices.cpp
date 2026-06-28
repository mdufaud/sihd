#include <gtest/gtest.h>

#include <sihd/sys/services.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::sys;
class TestServices: public ::testing::Test
{
    protected:
        TestServices() { sihd::util::LoggerManager::stream(); }

        virtual ~TestServices() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestServices, test_services_port_known)
{
    auto http = service_port("http");
    if (!http.has_value())
        GTEST_SKIP() << "no 'http' entry in services database";
    EXPECT_EQ(*http, 80u);

    auto https = service_port("https");
    if (!https.has_value())
        GTEST_SKIP() << "no 'https' entry in services database";
    EXPECT_EQ(*https, 443u);

    auto ftp = service_port("ftp");
    if (!ftp.has_value())
        GTEST_SKIP() << "no 'ftp' entry in services database";
    EXPECT_EQ(*ftp, 21u);
}

TEST_F(TestServices, test_services_port_unknown)
{
    EXPECT_EQ(service_port("definitely-not-a-real-service"), std::nullopt);
}

TEST_F(TestServices, test_services_name_known)
{
    auto name = service_name(80);
    if (!name.has_value())
        GTEST_SKIP() << "no entry for port 80 in services database";
    EXPECT_EQ(*name, "http");
}

TEST_F(TestServices, test_services_name_unknown)
{
    EXPECT_EQ(service_name(0), std::nullopt);
}

} // namespace test
