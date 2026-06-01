#include <gtest/gtest.h>

#include <sihd/json/Json.hpp>
#include <sihd/json/utils.hpp>

namespace test
{

using namespace sihd::json;

class TestOnDemand: public ::testing::Test
{
    protected:
        TestOnDemand() = default;

        virtual ~TestOnDemand() = default;

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestOnDemand, test_find_first_scalar_field)
{
    std::string data = R"([{"name":"alice","age":30},{"name":"bob","age":25}])";
    auto result = utils::find_first(data, "name");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_string());
    EXPECT_EQ(result->get<std::string>(), "alice");
}

TEST_F(TestOnDemand, test_find_first_key_absent_in_first_object)
{
    std::string data = R"([{"x":1},{"name":"bob"}])";
    auto result = utils::find_first(data, "name");
    // only the first object is examined, "name" is not there
    EXPECT_FALSE(result.has_value());
}

TEST_F(TestOnDemand, test_find_first_empty_array)
{
    std::string data = "[]";
    EXPECT_FALSE(utils::find_first(data, "key").has_value());
}

TEST_F(TestOnDemand, test_find_first_complex_value)
{
    std::string data = R"([{"scores":[100,200,300],"name":"alice"}])";
    auto result = utils::find_first(data, "scores");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_array());
    EXPECT_EQ(result->size(), 3u);
    EXPECT_EQ(result->operator[](0).get<int32_t>(), 100);
}

TEST_F(TestOnDemand, test_for_each_counts_all)
{
    std::string data = R"([{"a":1},{"b":2},{"c":3}])";
    size_t count = 0;
    utils::for_each(data, [&](Json j) -> bool {
        EXPECT_TRUE(j.is_object());
        ++count;
        return true;
    });
    EXPECT_EQ(count, 3u);
}

TEST_F(TestOnDemand, test_for_each_early_exit)
{
    std::string data = R"([{"v":1},{"v":2},{"v":3},{"v":4}])";
    size_t count = 0;
    utils::for_each(data, [&](Json) -> bool { return ++count < 2; });
    EXPECT_EQ(count, 2u);
}

} // namespace test
