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
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "key 'name' not found");
}

TEST_F(TestOnDemand, test_find_first_empty_array)
{
    auto result = utils::find_first("[]", "key");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "key 'key' not found");
}

TEST_F(TestOnDemand, test_find_first_invalid_document)
{
    auto result = utils::find_first("{not json", "key");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("not an array"), std::string::npos);
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
    auto walked = utils::for_each(data, [&](Json j) -> bool {
        EXPECT_TRUE(j.is_object());
        ++count;
        return true;
    });
    ASSERT_TRUE(walked.has_value());
    EXPECT_EQ(*walked, 3u);
    EXPECT_EQ(count, 3u);
}

TEST_F(TestOnDemand, test_for_each_early_exit)
{
    std::string data = R"([{"v":1},{"v":2},{"v":3},{"v":4}])";
    size_t count = 0;
    auto walked = utils::for_each(data, [&](Json) -> bool { return ++count < 2; });
    ASSERT_TRUE(walked.has_value());
    EXPECT_EQ(*walked, 2u);
    EXPECT_EQ(count, 2u);
}

TEST_F(TestOnDemand, test_for_each_unparsable_element_aborts)
{
    std::string data = R"([{"v":1},{nope},{"v":3}])";
    size_t count = 0;
    auto walked = utils::for_each(data, [&](Json) -> bool {
        ++count;
        return true;
    });
    // fail-fast: elements before the bad one are delivered, the walk aborts on the first unparsable
    ASSERT_FALSE(walked.has_value());
    EXPECT_EQ(count, 1u);
}

} // namespace test
