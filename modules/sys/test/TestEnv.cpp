#include <map>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/env.hpp>

#include "test_helper.hpp"

namespace test
{
using namespace sihd::sys;

TEST(TestEnv, test_get)
{
    ScopedEnv scoped("SIHD_TEST_VAR", "hello");

    EXPECT_EQ(env::get("SIHD_TEST_VAR").value_or(""), "hello");
    EXPECT_FALSE(env::get("SIHD_TEST_NOPE_VAR").has_value());
    EXPECT_FALSE(env::get("").has_value());
}

TEST(TestEnv, test_set_unset)
{
    EXPECT_TRUE(env::set("SIHD_TEST_VAR", "first"));
    EXPECT_EQ(env::get("SIHD_TEST_VAR").value_or(""), "first");

    EXPECT_TRUE(env::set("SIHD_TEST_VAR", "second"));
    EXPECT_EQ(env::get("SIHD_TEST_VAR").value_or(""), "second");

    EXPECT_TRUE(env::unset("SIHD_TEST_VAR"));
    EXPECT_FALSE(env::get("SIHD_TEST_VAR").has_value());
}

TEST(TestEnv, test_list)
{
    {
        ScopedEnv scoped("SIHD_TEST_VAR", "value");

        const std::map<std::string, std::string> map = env::list();
        EXPECT_FALSE(map.empty());
        EXPECT_EQ(map.at("SIHD_TEST_VAR"), "value");
        // present in any environment the tests run in (windows spells it 'Path')
        EXPECT_TRUE(map.contains("PATH") || map.contains("Path"));
    }

    const std::map<std::string, std::string> map = env::list();
    EXPECT_EQ(map.count("SIHD_TEST_VAR"), 0u);
}

TEST(TestEnv, test_expand)
{
    ScopedEnv scoped("SIHD_TEST_VAR", "hello");

    EXPECT_EQ(env::expand("no variable here"), "no variable here");
    EXPECT_EQ(env::expand("say $SIHD_TEST_VAR"), "say hello");
    EXPECT_EQ(env::expand("say ${SIHD_TEST_VAR}!"), "say hello!");
    EXPECT_EQ(env::expand("$SIHD_TEST_VAR$SIHD_TEST_VAR"), "hellohello");

    // unknown or malformed references are left as-is
    EXPECT_EQ(env::expand("$SIHD_TEST_NOPE_VAR/x"), "$SIHD_TEST_NOPE_VAR/x");
    EXPECT_EQ(env::expand("${SIHD_TEST_NOPE_VAR}"), "${SIHD_TEST_NOPE_VAR}");
    EXPECT_EQ(env::expand("${unterminated"), "${unterminated");
    EXPECT_EQ(env::expand("cost: 5$"), "cost: 5$");
    EXPECT_EQ(env::expand("$1var"), "$1var");
    EXPECT_EQ(env::expand("${}"), "${}");
}

} // namespace test
