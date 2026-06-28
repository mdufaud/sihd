#include <gtest/gtest.h>

#include <sihd/http/scheme.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::http;

class TestScheme: public ::testing::Test
{
    protected:
        TestScheme() { sihd::util::LoggerManager::stream(); }

        virtual ~TestScheme() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestScheme, test_scheme_is_ssl)
{
    EXPECT_TRUE(scheme_is_ssl("https"));
    EXPECT_TRUE(scheme_is_ssl("wss"));
    EXPECT_TRUE(scheme_is_ssl("ftps"));
    EXPECT_FALSE(scheme_is_ssl("http"));
    EXPECT_FALSE(scheme_is_ssl("ws"));
    EXPECT_FALSE(scheme_is_ssl("ftp"));
}

TEST_F(TestScheme, test_scheme_port_web)
{
    EXPECT_EQ(scheme_port("http"), 80);
    EXPECT_EQ(scheme_port("ws"), 80);
    EXPECT_EQ(scheme_port("https"), 443);
    EXPECT_EQ(scheme_port("wss"), 443);
}

TEST_F(TestScheme, test_scheme_port_service_db)
{
    int ftp = scheme_port("ftp");
    if (ftp == 0)
        GTEST_SKIP() << "no 'ftp' entry in services database";
    EXPECT_EQ(ftp, 21);
}

TEST_F(TestScheme, test_scheme_port_unknown)
{
    EXPECT_EQ(scheme_port("definitely-not-a-real-scheme"), 0);
}

} // namespace test
