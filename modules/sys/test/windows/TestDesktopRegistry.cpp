#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/desktop.hpp>
#include <sihd/sys/windows/registry.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::windows_::registry;

namespace
{

constexpr const char *personalize_key = "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr const char *light_theme_value = "AppsUseLightTheme";

} // namespace

class TestDesktopRegistry: public ::testing::Test
{
    protected:
        TestDesktopRegistry() { LoggerManager::stream(); }

        virtual ~TestDesktopRegistry() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            auto created = _key.create(Hive::current_user, personalize_key);
            ASSERT_TRUE(created.has_value()) << created.error().message;
            auto previous = _key.u32(light_theme_value);
            if (previous.has_value())
                _previous = *previous;
        }

        virtual void TearDown()
        {
            if (_previous.has_value())
                (void)_key.set_u32(light_theme_value, *_previous);
            else
                (void)_key.remove_value(light_theme_value);
        }

        Key _key;
        std::optional<uint32_t> _previous;
};

TEST_F(TestDesktopRegistry, test_color_scheme)
{
    ASSERT_TRUE(_key.set_u32(light_theme_value, 0).has_value());
    EXPECT_EQ(desktop::color_scheme().value_or(desktop::ColorScheme::no_preference), desktop::ColorScheme::dark);

    ASSERT_TRUE(_key.set_u32(light_theme_value, 1).has_value());
    EXPECT_EQ(desktop::color_scheme().value_or(desktop::ColorScheme::no_preference), desktop::ColorScheme::light);

    (void)_key.remove_value(light_theme_value);
    EXPECT_EQ(desktop::color_scheme().value_or(desktop::ColorScheme::no_preference), desktop::ColorScheme::light);
}

} // namespace test
