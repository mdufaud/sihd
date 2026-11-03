#include <gtest/gtest.h>

#include <sihd/sys/PowerInhibitor.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestPowerInhibitor: public ::testing::Test
{
    protected:
        TestPowerInhibitor() { LoggerManager::stream(); }

        virtual ~TestPowerInhibitor() { LoggerManager::clear_loggers(); }
};

TEST_F(TestPowerInhibitor, test_sleep)
{
    PowerInhibitor inhibitor;

    auto inhibited = inhibitor.inhibit_sleep("Recording");

    ASSERT_TRUE(inhibited.has_value()) << inhibited.error().message;
    EXPECT_TRUE(inhibitor.is_sleep_inhibited());
    EXPECT_FALSE(inhibitor.is_display_inhibited());

    auto twice = inhibitor.inhibit_sleep("Recording");

    ASSERT_FALSE(twice.has_value());
    EXPECT_EQ(twice.error().code, ErrorCode::already_exists);
    EXPECT_EQ(twice.error().message, "sleep is already inhibited");

    auto released = inhibitor.release();

    EXPECT_TRUE(released.has_value()) << released.error().message;
    EXPECT_FALSE(inhibitor.is_sleep_inhibited());
}

TEST_F(TestPowerInhibitor, test_display)
{
    PowerInhibitor inhibitor;

    auto inhibited = inhibitor.inhibit_display("Slideshow");

    ASSERT_TRUE(inhibited.has_value()) << inhibited.error().message;
    EXPECT_TRUE(inhibitor.is_display_inhibited());
    EXPECT_FALSE(inhibitor.is_sleep_inhibited());

    auto twice = inhibitor.inhibit_display("Slideshow");

    ASSERT_FALSE(twice.has_value());
    EXPECT_EQ(twice.error().code, ErrorCode::already_exists);
    EXPECT_EQ(twice.error().message, "display is already inhibited");

    auto released = inhibitor.release();

    EXPECT_TRUE(released.has_value()) << released.error().message;
    EXPECT_FALSE(inhibitor.is_display_inhibited());
}

TEST_F(TestPowerInhibitor, test_release_without_inhibition)
{
    PowerInhibitor inhibitor;

    EXPECT_TRUE(inhibitor.release().has_value());
}

} // namespace test
