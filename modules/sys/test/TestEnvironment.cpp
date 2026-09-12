#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/Environment.hpp>

#include "test_helper.hpp"

namespace test
{
using namespace sihd::sys;

TEST(TestEnvironment, test_set_get)
{
    Environment env;

    EXPECT_TRUE(env.empty());
    EXPECT_EQ(env.size(), 0u);

    env.set("A", "1");
    env.set("B", "2");
    EXPECT_FALSE(env.empty());
    EXPECT_EQ(env.size(), 2u);

    EXPECT_EQ(env.get("A").value_or(""), "1");
    EXPECT_EQ(env.get("B").value_or(""), "2");
    EXPECT_FALSE(env.get("C").has_value());

    env.set("A", "overwritten");
    EXPECT_EQ(env.size(), 2u);
    EXPECT_EQ(env.get("A").value_or(""), "overwritten");
}

TEST(TestEnvironment, test_rm_clear)
{
    Environment env;
    env.set("A", "1");
    env.set("B", "2");

    EXPECT_TRUE(env.rm("A"));
    EXPECT_FALSE(env.get("A").has_value());
    EXPECT_FALSE(env.rm("A"));
    EXPECT_EQ(env.size(), 1u);

    env.clear();
    EXPECT_TRUE(env.empty());
    EXPECT_FALSE(env.get("B").has_value());
}

TEST(TestEnvironment, test_load)
{
    Environment env;
    env.set("OLD", "gone");
    env.load({"A=1", "B=2", "OLD=3", "no delimiter", "=no key"});

    EXPECT_EQ(env.size(), 3u);
    EXPECT_EQ(env.get("A").value_or(""), "1");
    EXPECT_EQ(env.get("B").value_or(""), "2");
    // load replaces existing keys
    EXPECT_EQ(env.get("OLD").value_or(""), "3");

    const std::vector<std::string> entries = {"C=4"};
    env.load(entries);
    EXPECT_EQ(env.get("C").value_or(""), "4");

    std::vector<std::string_view> views = {"D=5"};
    env.load(views);
    EXPECT_EQ(env.get("D").value_or(""), "5");

    const char *raw[] = {"E=6"};
    env.load(raw);
    EXPECT_EQ(env.get("E").value_or(""), "6");
}

TEST(TestEnvironment, test_from_current)
{
    ScopedEnv scoped("SIHD_TEST_VAR", "snapshot");

    const Environment env = Environment::from_current();
    EXPECT_EQ(env.get("SIHD_TEST_VAR").value_or(""), "snapshot");
    // present in any environment the tests run in (windows spells it 'Path')
    EXPECT_TRUE(env.get("PATH").has_value() || env.get("Path").has_value());
}

TEST(TestEnvironment, test_expand)
{
    Environment env;
    env.set("SIHD_TEST_VAR", "hello");

    EXPECT_EQ(env.expand("no variable here"), "no variable here");
    EXPECT_EQ(env.expand("say $SIHD_TEST_VAR"), "say hello");
    EXPECT_EQ(env.expand("say ${SIHD_TEST_VAR}!"), "say hello!");
    EXPECT_EQ(env.expand("$SIHD_TEST_VAR$SIHD_TEST_VAR"), "hellohello");

    // unknown or malformed references are left as-is
    EXPECT_EQ(env.expand("$SIHD_TEST_NOPE_VAR/x"), "$SIHD_TEST_NOPE_VAR/x");
    EXPECT_EQ(env.expand("${SIHD_TEST_NOPE_VAR}"), "${SIHD_TEST_NOPE_VAR}");
    EXPECT_EQ(env.expand("${unterminated"), "${unterminated");
    EXPECT_EQ(env.expand("cost: 5$"), "cost: 5$");
    EXPECT_EQ(env.expand("$1var"), "$1var");
    EXPECT_EQ(env.expand("${}"), "${}");

    env.rm("SIHD_TEST_VAR");
    EXPECT_EQ(env.expand("$SIHD_TEST_VAR"), "$SIHD_TEST_VAR");
}

TEST(TestEnvironment, test_entries)
{
    Environment env;
    env.set("A", "1");
    env.set("B", "2");

    const std::map<std::string, std::string> & entries = env.entries();
    EXPECT_EQ(entries.at("A"), "1");
    EXPECT_EQ(entries.at("B"), "2");
}

} // namespace test
