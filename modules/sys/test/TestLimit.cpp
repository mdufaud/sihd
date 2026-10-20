#include <gtest/gtest.h>

#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sihd/sys/linux/limit.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;

class TestLimit: public ::testing::Test
{
    protected:
        TestLimit() { LoggerManager::stream(); }

        virtual ~TestLimit() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!limit::supported)
            {
                GTEST_SKIP() << "limit not supported on this platform";
            }
            auto nofile = limit::get(limit::Resource::nofile);
            ASSERT_TRUE(nofile.has_value()) << nofile.error().message;
            _nofile = *nofile;
            auto core = limit::get(limit::Resource::core);
            ASSERT_TRUE(core.has_value()) << core.error().message;
            _core = *core;
            _saved = true;
        }

        virtual void TearDown()
        {
            if (!_saved)
                return;
            (void)limit::set(limit::Resource::nofile, _nofile.current, _nofile.maximum);
            (void)limit::set(limit::Resource::core, _core.current, _core.maximum);
        }

        limit::Limit _nofile;
        limit::Limit _core;
        bool _saved = false;
};

TEST_F(TestLimit, test_set_nofile)
{
    if (_nofile.maximum < 64)
        GTEST_SKIP() << "nofile hard limit below 64";

    ASSERT_TRUE(limit::set(limit::Resource::nofile, 64, _nofile.maximum));

    auto nofile = limit::get(limit::Resource::nofile);
    ASSERT_TRUE(nofile.has_value());
    EXPECT_EQ(nofile->current, 64u);
    EXPECT_EQ(nofile->maximum, _nofile.maximum);
    auto max_fds = os::max_fds();
    ASSERT_TRUE(max_fds.has_value());
    EXPECT_EQ(*max_fds, 64u);
}

TEST_F(TestLimit, test_lower_core)
{
    ASSERT_TRUE(limit::set(limit::Resource::core, 0, _core.maximum));

    auto lowered = limit::get(limit::Resource::core);
    ASSERT_TRUE(lowered.has_value());
    EXPECT_EQ(lowered->current, 0u);
    EXPECT_EQ(lowered->maximum, _core.maximum);
}

TEST_F(TestLimit, test_errors)
{
    if (_nofile.maximum == limit::infinity)
        GTEST_SKIP() << "nofile hard limit is infinite";

    auto inverted = limit::set(limit::Resource::nofile, _nofile.maximum, _nofile.maximum - 1);
    ASSERT_FALSE(inverted.has_value());
    EXPECT_EQ(inverted.error().code, ErrorCode::invalid_argument);
}

} // namespace test

#endif
