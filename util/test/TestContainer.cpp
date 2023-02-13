#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>

namespace test
{
    SIHD_LOGGER;

    using namespace sihd::util;
    class TestContainer: public ::testing::Test
    {
        protected:
            TestContainer()
            {
                LoggerManager::basic();
            }

            virtual ~TestContainer()
            {
                LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestContainer, test_container_map)
    {
        std::map<std::string, int> map = {
            {"c", 3},
            {"a", 1},
            {"b", 2},
        };

        std::vector<std::string> ordered_keys{"a", "b", "c"};
        EXPECT_EQ(container::ordered_keys(map), ordered_keys);
        EXPECT_EQ(container::get_or(map, "c", 42), 3);
        EXPECT_EQ(container::get_or(map, "d", 42), 42);

        using NestedMapType = std::map<std::string, std::map<std::string, int>>;
        NestedMapType nested_map = {
            {"first", {
                {"one", 1},
                {"two", 2}
            }},
        };

        EXPECT_NE(container::recursive_search(nested_map, "first", "one"), nullptr);
        EXPECT_EQ(container::recursive_search(nested_map, "NOPE", "one"), nullptr);
        EXPECT_EQ(container::recursive_search(nested_map, "first", "NOPE"), nullptr);

        EXPECT_EQ(container::recursive_get(nested_map, "first", "one"), 1);
        // should throw if map is const
        EXPECT_THROW(container::recursive_get(static_cast<const NestedMapType>(nested_map), "NOPE", "one"), std::out_of_range);
        EXPECT_THROW(container::recursive_get(static_cast<const NestedMapType>(nested_map), "first", "NOPE"), std::out_of_range);
        // creates an entry if map is not const
        EXPECT_THROW(nested_map.at("NONE").at("NONE2"), std::out_of_range);
        EXPECT_EQ(container::recursive_get(nested_map, "NONE", "NONE2"), 0);
        EXPECT_NO_THROW(nested_map.at("NONE").at("NONE2"));
    }

    TEST_F(TestContainer, test_container_vector)
    {
        std::vector<int> vec = {1, 2, 3, 4};

        EXPECT_TRUE(container::contains(vec, 2));
        EXPECT_FALSE(container::contains(vec, 5));

        EXPECT_TRUE(container::emplace_unique(vec, 5));
        EXPECT_FALSE(container::emplace_unique(vec, 4));
        EXPECT_EQ(vec.size(), 5u);

        EXPECT_FALSE(container::erase(vec, 0));
        EXPECT_TRUE(container::erase(vec, 3));
        EXPECT_EQ(vec.size(), 4u);
    }

    TEST_F(TestContainer, test_container_str)
    {
        EXPECT_EQ(container::str(std::map<std::string, int>{}), "{}");
        EXPECT_EQ(container::str(std::map<std::string, int>{
            {"first", 1},
        }), "{first: 1}");
        EXPECT_EQ(container::str(std::map<std::string, int>{
            {"first", 1},
            {"second", 2},
        }), "{first: 1, second: 2}");
        EXPECT_EQ(container::str(std::map<int, int>{
            {1, 42},
            {2, 1337},
        }), "{1: 42, 2: 1337}");

        EXPECT_EQ(container::str(std::vector<int>{}), "[]");
        EXPECT_EQ(container::str(std::vector<int>{1, 2, 3}), "[1, 2, 3]");
        EXPECT_EQ(container::str(std::vector<std::string>{"hi"}), "[hi]");
        EXPECT_EQ(container::str(std::array<std::string, 2>{"hi", "!"}), "[hi, !]");
    }
}
