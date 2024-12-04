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
        TestContainer() { LoggerManager::stream(); }

        virtual ~TestContainer() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestContainer, test_container_map)
{
    using Map = std::map<std::string, int>;
    Map map = {
        {"c", 3},
        {"a", 1},
        {"b", 2},
    };

    std::vector<std::string> ordered_keys {"a", "b", "c"};

    std::sort(ordered_keys.begin(), ordered_keys.end(), std::less<std::string>());
    EXPECT_EQ(container::ordered_keys(map), ordered_keys);

    std::sort(ordered_keys.begin(), ordered_keys.end(), std::greater<std::string>());
    EXPECT_EQ(container::ordered_keys(map, std::greater<std::string>()), ordered_keys);

    EXPECT_EQ(container::get_or(map, "c", 42), 3);
    EXPECT_EQ(container::get_or(map, "d", 42), 42);

    using NestedMapType = std::map<std::string, Map>;
    NestedMapType nested_map = {
        {"first", {{"one", 1}, {"two", 2}}},
    };

    EXPECT_NE(container::recursive_map_search(nested_map, "first", "one"), nullptr);
    EXPECT_EQ(container::recursive_map_search(nested_map, "NOPE", "one"), nullptr);
    EXPECT_EQ(container::recursive_map_search(nested_map, "first", "NOPE"), nullptr);

    EXPECT_EQ(container::recursive_get(nested_map, "first", "one"), 1);
    // should throw if map is const
    EXPECT_THROW(container::recursive_get(static_cast<const NestedMapType>(nested_map), "NOPE", "one"),
                 std::out_of_range);
    EXPECT_THROW(container::recursive_get(static_cast<const NestedMapType>(nested_map), "first", "NOPE"),
                 std::out_of_range);
    // creates an entry if map is not const
    EXPECT_THROW(nested_map.at("NONE").at("NONE2"), std::out_of_range);
    EXPECT_EQ(container::recursive_get(nested_map, "NONE", "NONE2"), 0);
    EXPECT_NO_THROW(nested_map.at("NONE").at("NONE2"));

    using VectorMap = std::vector<Map>;
    VectorMap vector_map;
    vector_map.emplace_back(map);
    map["d"] = 4;
    vector_map.emplace_back(map);

    EXPECT_EQ(container::recursive_get(vector_map, 0, "a"), 1);
    EXPECT_EQ(container::recursive_get(vector_map, 1, "d"), 4);
}

TEST_F(TestContainer, test_container_vector)
{
    std::vector<int> vec = {1, 2, 3, 4};

    EXPECT_TRUE(container::contains(vec, 2));
    EXPECT_FALSE(container::contains(vec, 5));

    EXPECT_TRUE(container::emplace_unique(vec, 5));
    EXPECT_FALSE(container::emplace_unique(vec, 4));
    EXPECT_EQ(vec.size(), 5u);

    // 1 2 3 4 5

    EXPECT_FALSE(container::erase(vec, 0));
    EXPECT_TRUE(container::erase(vec, 3));
    EXPECT_EQ(vec.size(), 4u);

    // 1 2 4 5
    const int total_vector = 12;

    EXPECT_EQ(container::sum(vec), total_vector);
    // EXPECT_EQ(container::sum(vec, [](auto val) { return val * 2; }), total_vector * 2);

    EXPECT_FLOAT_EQ(container::average(vec), total_vector / vec.size());

    EXPECT_FALSE(container::contains(vec, 6));
    container::insert_at_end(vec, std::vector<int> {6, 7});
    ASSERT_EQ(vec.size(), 6u);
    EXPECT_EQ(vec[4], 6);
    EXPECT_EQ(vec[5], 7);
}
} // namespace test
