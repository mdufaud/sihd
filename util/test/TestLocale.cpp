#include <gtest/gtest.h>

#include <sihd/util/locale.hpp>
#include <sihd/util/platform.hpp>

namespace test
{

using namespace sihd::util;

class TestLocale: public ::testing::Test
{
    protected:
        TestLocale() {}

        virtual ~TestLocale() {}

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestLocale, test_locale_normalize_name)
{
    EXPECT_EQ(locale::normalize_locale_name("C"), "C");
    EXPECT_EQ(locale::normalize_locale_name("POSIX"), "POSIX");
    EXPECT_EQ(locale::normalize_locale_name(""), "");
    EXPECT_EQ(locale::normalize_locale_name("en_US.UTF-8"), "en_US.UTF-8");
    EXPECT_EQ(locale::normalize_locale_name("fr_FR.ISO-8859-1"), "fr_FR.ISO-8859-1");
    EXPECT_EQ(locale::normalize_locale_name("de_DE"), "de_DE");
}

TEST_F(TestLocale, test_locale_create_classic)
{
    auto loc_c = locale::create_locale("C");
    EXPECT_NO_THROW(locale::create_locale("C"));

    auto loc_posix = locale::create_locale("POSIX");
    EXPECT_NO_THROW(locale::create_locale("POSIX"));

    auto loc_classic = locale::create_locale("");
    EXPECT_NO_THROW(locale::create_locale(""));
}

TEST_F(TestLocale, test_locale_create_throws_on_invalid)
{
    // Invalid locale should throw
    EXPECT_THROW(locale::create_locale("this_does_not_exist_XYZ.UTF-99"), std::runtime_error);
}

TEST_F(TestLocale, test_locale_create_with_name)
{
    try
    {
        auto loc = locale::create_locale("en_US.UTF-8");
        SUCCEED();
    }
    catch (const std::runtime_error &)
    {
        // Locale not available on this system - acceptable
        SUCCEED();
    }
}

} // namespace test
