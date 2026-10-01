#include <gtest/gtest.h>

#include <sihd/ssh/utils.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;

class TestSshUtils: public ::testing::Test
{
    protected:
        TestSshUtils() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSshUtils() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshUtils, test_ssh_utils_init_finalize)
{
    // Drain any leaked init from previous tests
    int drained = 0;
    while (utils::is_initialized())
    {
        static_cast<void>(utils::finalize());
        drained++;
    }
    if (drained > 0)
    {
        FAIL() << "Had to drain " << drained << " leaked utils::init() calls from previous tests";
    }

    EXPECT_FALSE(utils::is_initialized());

    // First init
    EXPECT_TRUE(utils::init().has_value());
    EXPECT_TRUE(utils::is_initialized());

    // Second init - should increment counter
    EXPECT_TRUE(utils::init().has_value());
    EXPECT_TRUE(utils::is_initialized());

    // Third init
    EXPECT_TRUE(utils::init().has_value());
    EXPECT_TRUE(utils::is_initialized());

    // First finalize - should not call ssh_finalize yet
    EXPECT_TRUE(utils::finalize().has_value());
    EXPECT_TRUE(utils::is_initialized());

    // Second finalize
    EXPECT_TRUE(utils::finalize().has_value());
    EXPECT_TRUE(utils::is_initialized());

    // Third finalize - should call ssh_finalize
    EXPECT_TRUE(utils::finalize().has_value());
    EXPECT_FALSE(utils::is_initialized());

    // Finalize when already at 0 should be a not_initialized error
    auto res = utils::finalize();
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, not_initialized);
    EXPECT_FALSE(utils::is_initialized());

    // Re-init after finalize should work
    EXPECT_TRUE(utils::init().has_value());
    EXPECT_TRUE(utils::is_initialized());
    EXPECT_TRUE(utils::finalize().has_value());
    EXPECT_FALSE(utils::is_initialized());
}

} // namespace test
