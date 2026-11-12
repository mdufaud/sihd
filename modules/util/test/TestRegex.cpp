#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Regex.hpp>

namespace test
{
using namespace sihd::util;

class TestRegex: public ::testing::Test
{
    protected:
        TestRegex() { LoggerManager::stream(); }

        virtual ~TestRegex() { LoggerManager::clear_loggers(); }
};

TEST_F(TestRegex, test_regex_uncompiled)
{
    const Regex regex;

    auto matched = regex.matches("abc");
    auto found = regex.find_all("abc");
    auto replaced = regex.replace_all("abc", "x");

    EXPECT_FALSE(regex.is_compiled());
    ASSERT_FALSE(matched.has_value());
    EXPECT_EQ(matched.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(matched.error().message, "regex not compiled");
    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().code, ErrorCode::not_initialized);
    ASSERT_FALSE(replaced.has_value());
    EXPECT_EQ(replaced.error().code, ErrorCode::not_initialized);
}

TEST_F(TestRegex, test_regex_failed_compile_keeps_pattern)
{
    Regex regex("b+");

    auto refused = regex.compile("(", Regex::Mode::full);
    auto found = regex.find_all("abbcb");

    EXPECT_TRUE(regex.is_compiled());
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::invalid_argument);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, std::vector<std::string>({"bb", "b"}));
    EXPECT_THROW({ Regex invalid("("); }, std::runtime_error);
}

} // namespace test
