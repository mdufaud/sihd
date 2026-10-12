#include <gtest/gtest.h>

#include <sihd/sys/session.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestSession: public ::testing::Test
{
    protected:
        TestSession() { LoggerManager::stream(); }

        virtual ~TestSession() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSession, test_session_list)
{
    const auto sessions = session::sessions();
    for (const auto & session : sessions)
    {
        EXPECT_FALSE(session.username.empty());
        EXPECT_FALSE(session.device.empty());
        SIHD_LOG(debug,
                 "session {}: '{}' on '{}' from '{}'",
                 session.id,
                 session.username,
                 session.device,
                 session.hostname);
    }
}

} // namespace test
