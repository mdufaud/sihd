#include <gtest/gtest.h>

#include <sihd/util/StrConfiguration.hpp>

namespace test
{
using namespace sihd::util;

class TestStrConfiguration: public ::testing::Test
{
    protected:
        TestStrConfiguration() = default;
        virtual ~TestStrConfiguration() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestStrConfiguration, test_strconf_parse)
{
    StrConfiguration conf("key1=val1;key2=val2;key3=val3");

    EXPECT_EQ(conf.size(), 3u);
    EXPECT_FALSE(conf.empty());
    EXPECT_TRUE(conf.has("key1"));
    EXPECT_TRUE(conf.has("key2"));
    EXPECT_TRUE(conf.has("key3"));
    EXPECT_FALSE(conf.has("key4"));

    EXPECT_EQ(conf.get("key1"), "val1");
    EXPECT_EQ(conf.get("key2"), "val2");
    EXPECT_EQ(conf["key3"], "val3");
    EXPECT_EQ(conf["nonexistent"], "");
}

TEST_F(TestStrConfiguration, test_strconf_find)
{
    StrConfiguration conf("a=1;b=2");

    auto opt = conf.find("a");
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, "1");

    EXPECT_FALSE(conf.find("z").has_value());
}

TEST_F(TestStrConfiguration, test_strconf_find_all)
{
    StrConfiguration conf("x=10;y=20;z=30");

    auto [a, b, c] = conf.find_all("x", "y", "missing");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(*a, "10");
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(*b, "20");
    EXPECT_FALSE(c.has_value());
}

TEST_F(TestStrConfiguration, test_strconf_throws)
{
    StrConfiguration conf("a=1");
    EXPECT_THROW(conf.get("nonexistent"), std::out_of_range);
}

TEST_F(TestStrConfiguration, test_strconf_empty)
{
    StrConfiguration conf("");
    EXPECT_TRUE(conf.empty());
    EXPECT_EQ(conf.size(), 0u);
}

TEST_F(TestStrConfiguration, test_strconf_roundtrip)
{
    StrConfiguration conf("a=1;b=2");
    std::string s = conf.str();
    StrConfiguration conf2(s);
    EXPECT_EQ(conf2.get("a"), "1");
    EXPECT_EQ(conf2.get("b"), "2");
}

} // namespace test
