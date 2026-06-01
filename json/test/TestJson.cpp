#include <gtest/gtest.h>

#include <sihd/json/Json.hpp>

namespace test
{

using namespace sihd::json;

class TestJson: public ::testing::Test
{
    protected:
        TestJson() = default;

        virtual ~TestJson() = default;

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestJson, test_json_null)
{
    Json j;
    EXPECT_TRUE(j.is_null());
    EXPECT_FALSE(j.is_bool());
    EXPECT_FALSE(j.is_number());
    EXPECT_FALSE(j.is_string());
    EXPECT_FALSE(j.is_array());
    EXPECT_FALSE(j.is_object());
    EXPECT_FALSE(j.is_discarded());
    EXPECT_EQ(j.dump(), "null");

    Json j2(nullptr);
    EXPECT_TRUE(j2.is_null());
    EXPECT_EQ(j2.dump(), "null");
}

TEST_F(TestJson, test_json_bool)
{
    Json jt(true);
    EXPECT_TRUE(jt.is_bool());
    EXPECT_EQ(jt.get<bool>(), true);
    EXPECT_EQ(jt.dump(), "true");

    Json jf(false);
    EXPECT_TRUE(jf.is_bool());
    EXPECT_EQ(jf.get<bool>(), false);
    EXPECT_EQ(jf.dump(), "false");
}

TEST_F(TestJson, test_json_integers)
{
    Json j_int32(42);
    EXPECT_TRUE(j_int32.is_number_integer());
    EXPECT_EQ(j_int32.get<int32_t>(), 42);
    EXPECT_EQ(j_int32.get<int64_t>(), 42);
    EXPECT_EQ(j_int32.dump(), "42");

    Json j_neg(-123);
    EXPECT_TRUE(j_neg.is_number_integer());
    EXPECT_EQ(j_neg.get<int64_t>(), -123);

    Json j_uint(100u);
    EXPECT_TRUE(j_uint.is_number_unsigned());
    EXPECT_EQ(j_uint.get<uint32_t>(), 100u);
    EXPECT_EQ(j_uint.get<uint64_t>(), 100u);

    Json j_int64(int64_t(9876543210));
    EXPECT_TRUE(j_int64.is_number_integer());
    EXPECT_EQ(j_int64.get<int64_t>(), 9876543210);

    Json j_uint64(uint64_t(18000000000000000000ULL));
    EXPECT_TRUE(j_uint64.is_number_unsigned());
    EXPECT_EQ(j_uint64.get<uint64_t>(), 18000000000000000000ULL);
}

TEST_F(TestJson, test_json_float)
{
    Json j_float(3.14f);
    EXPECT_TRUE(j_float.is_number_float());
    EXPECT_FLOAT_EQ(j_float.get<float>(), 3.14f);

    Json j_double(2.71828);
    EXPECT_TRUE(j_double.is_number_float());
    EXPECT_DOUBLE_EQ(j_double.get<double>(), 2.71828);
}

TEST_F(TestJson, test_json_string)
{
    Json j_cstr("hello");
    EXPECT_TRUE(j_cstr.is_string());
    EXPECT_EQ(j_cstr.get<std::string>(), "hello");

    Json j_str(std::string("world"));
    EXPECT_TRUE(j_str.is_string());
    EXPECT_EQ(j_str.get<std::string>(), "world");

    Json j_sv(std::string_view("view"));
    EXPECT_TRUE(j_sv.is_string());
    EXPECT_EQ(j_sv.get<std::string>(), "view");

    EXPECT_EQ(j_cstr.dump(), "\"hello\"");
}

TEST_F(TestJson, test_json_initializer_list_object)
{
    Json j = {{"key1", 42}, {"key2", "value"}, {"key3", true}};
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j.size(), 3u);

    EXPECT_TRUE(j["key1"].is_number_integer());
    EXPECT_EQ(j["key1"].get<int32_t>(), 42);

    EXPECT_TRUE(j["key2"].is_string());
    EXPECT_EQ(j["key2"].get<std::string>(), "value");

    EXPECT_TRUE(j["key3"].is_bool());
    EXPECT_EQ(j["key3"].get<bool>(), true);

    EXPECT_TRUE(j["nonexistent"].is_null());
}

TEST_F(TestJson, test_json_initializer_list_array)
{
    Json j = {1, 2, 3};
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_EQ(j[0].get<int32_t>(), 1);
    EXPECT_EQ(j[1].get<int32_t>(), 2);
    EXPECT_EQ(j[2].get<int32_t>(), 3);
}

TEST_F(TestJson, test_json_copy_move)
{
    Json original = {{"key", "value"}, {"num", 42}};

    Json copied(original);
    EXPECT_TRUE(copied.is_object());
    EXPECT_EQ(copied["key"].get<std::string>(), "value");
    EXPECT_EQ(copied["num"].get<int32_t>(), 42);

    Json moved(std::move(copied));
    EXPECT_TRUE(moved.is_object());
    EXPECT_EQ(moved["key"].get<std::string>(), "value");

    Json assigned;
    assigned = original;
    EXPECT_TRUE(assigned.is_object());
    EXPECT_EQ(assigned["num"].get<int32_t>(), 42);
}

TEST_F(TestJson, test_json_parse_object)
{
    auto j = Json::parse(R"({"name": "test", "value": 42, "active": true})");
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j["name"].get<std::string>(), "test");
    EXPECT_EQ(j["value"].get<int32_t>(), 42);
    EXPECT_EQ(j["active"].get<bool>(), true);
}

TEST_F(TestJson, test_json_parse_array)
{
    auto j = Json::parse("[1, 2, 3]");
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_EQ(j[0].get<int32_t>(), 1);
}

TEST_F(TestJson, test_json_parse_error)
{
    auto j = Json::parse("{invalid}", false);
    EXPECT_TRUE(j.is_discarded());

    EXPECT_THROW(Json::parse("{invalid}"), std::runtime_error);
}

TEST_F(TestJson, test_json_parse_pointer_range)
{
    std::string data = R"({"hello": "world"})";
    auto j = Json::parse(data.data(), data.data() + data.size(), false);
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j["hello"].get<std::string>(), "world");
}

TEST_F(TestJson, test_json_dump)
{
    Json j = {{"bool", true}, {"int", 1234}, {"str", "hello"}};
    std::string compact = j.dump();
    EXPECT_FALSE(compact.empty());

    std::string pretty = j.dump(2);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
}

TEST_F(TestJson, test_json_dump_parsed_indent)
{
    auto j = Json::parse(R"({"a": 1, "b": [2, 3]})");

    EXPECT_EQ(j.dump(), R"({"a":1,"b":[2,3]})");

    EXPECT_EQ(j.dump(2), "{\n"
                          "  \"a\": 1,\n"
                          "  \"b\": [\n"
                          "    2,\n"
                          "    3\n"
                          "  ]\n"
                          "}");

    EXPECT_EQ(j.dump(4), "{\n"
                          "    \"a\": 1,\n"
                          "    \"b\": [\n"
                          "        2,\n"
                          "        3\n"
                          "    ]\n"
                          "}");
}

TEST_F(TestJson, test_json_contains)
{
    Json j = {{"key", 42}, {"null_key", nullptr}};
    EXPECT_TRUE(j.contains("key"));
    EXPECT_TRUE(j.contains("null_key"));
    EXPECT_FALSE(j.contains("missing"));

    EXPECT_FALSE(Json(42).contains("x"));
    EXPECT_FALSE(Json().contains("x"));

    auto parsed = Json::parse(R"({"a": 1, "b": null})");
    EXPECT_TRUE(parsed.contains("a"));
    EXPECT_TRUE(parsed.contains("b"));
    EXPECT_FALSE(parsed.contains("c"));
}

TEST_F(TestJson, test_json_get_or)
{
    Json j = {{"name", "alice"}, {"age", 30}};

    EXPECT_EQ(j["name"].get_or<std::string>("default"), "alice");
    EXPECT_EQ(j["missing"].get_or<std::string>("default"), "default");
    EXPECT_EQ(j["age"].get_or<int32_t>(0), 30);
    EXPECT_EQ(j["missing"].get_or<int32_t>(99), 99);

    EXPECT_EQ(j["name"].get_or<int32_t>(42), 42);
}

TEST_F(TestJson, test_json_equality)
{
    EXPECT_EQ(Json(), Json(nullptr));
    EXPECT_EQ(Json(true), Json(true));
    EXPECT_EQ(Json(42), Json(42));
    EXPECT_EQ(Json("hello"), Json("hello"));

    EXPECT_FALSE(Json(true) == Json(false));
    EXPECT_FALSE(Json(1) == Json(2));
    EXPECT_FALSE(Json("a") == Json("b"));
    EXPECT_FALSE(Json(42) == Json("42"));

    Json arr1 = {1, 2, 3};
    Json arr2 = {1, 2, 3};
    Json arr3 = {1, 2, 4};
    EXPECT_EQ(arr1, arr2);
    EXPECT_FALSE(arr1 == arr3);

    Json obj1 = {{"a", 1}, {"b", 2}};
    Json obj2 = {{"a", 1}, {"b", 2}};
    Json obj3 = {{"a", 1}, {"b", 3}};
    EXPECT_EQ(obj1, obj2);
    EXPECT_FALSE(obj1 == obj3);

    auto parsed = Json::parse(R"({"x": [1, 2]})");
    Json built = {{"x", {1, 2}}};
    EXPECT_EQ(parsed, built);
}

TEST_F(TestJson, test_json_iteration_object)
{
    Json j = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_TRUE(j.is_object());

    std::vector<std::string> keys;
    std::vector<int> values;
    for (auto it = j.begin(); it != j.end(); ++it)
    {
        keys.push_back(it.key());
        values.push_back(it.value().get<int32_t>());
    }
    EXPECT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "b");
    EXPECT_EQ(keys[2], "c");
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST_F(TestJson, test_json_iteration_array)
{
    Json j = {10, 20, 30};
    EXPECT_TRUE(j.is_array());

    std::vector<int> values;
    for (const auto & elem : j)
    {
        values.push_back(elem.get<int32_t>());
    }
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
}

} // namespace test
