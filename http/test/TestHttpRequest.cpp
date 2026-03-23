#include <gtest/gtest.h>
#include <sihd/json/Json.hpp>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::http;

class TestHttpRequest: public ::testing::Test
{
    protected:
        TestHttpRequest() { sihd::util::LoggerManager::stream(); }
        virtual ~TestHttpRequest() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestHttpRequest, test_request_types)
{
    // bidirectional string ↔ enum conversion, case-insensitive
    EXPECT_EQ(HttpRequest::type_from_str("get"), HttpRequest::Get);
    EXPECT_EQ(HttpRequest::type_from_str("POST"), HttpRequest::Post);
    EXPECT_EQ(HttpRequest::type_from_str("Put"), HttpRequest::Put);
    EXPECT_EQ(HttpRequest::type_from_str("DELETE"), HttpRequest::Delete);
    EXPECT_EQ(HttpRequest::type_from_str("OPTIONS"), HttpRequest::Options);
    EXPECT_EQ(HttpRequest::type_from_str("unknown"), HttpRequest::None);

    EXPECT_EQ(HttpRequest::type_str(HttpRequest::Get), "GET");
    EXPECT_EQ(HttpRequest::type_str(HttpRequest::Post), "POST");
    EXPECT_EQ(HttpRequest::type_str(HttpRequest::None), "None");

    HttpRequest req("/path", HttpRequest::Delete);
    EXPECT_EQ(req.type_str(), "DELETE");
}

TEST_F(TestHttpRequest, test_content)
{
    HttpRequest req("/upload", HttpRequest::Post);
    EXPECT_FALSE(req.has_content());

    std::string body = R"({"key":"val","n":7})";
    req.set_content(sihd::util::ArrCharView(body.data(), body.size()));
    EXPECT_TRUE(req.has_content());
    EXPECT_EQ(req.content().cpp_str(), body);

    // valid json roundtrip
    auto parsed = req.content_as_json();
    EXPECT_FALSE(parsed.is_discarded());
    EXPECT_EQ(parsed["key"].get<std::string>(), "val");
    EXPECT_EQ(parsed["n"].get<int32_t>(), 7);

    // invalid json returns discarded
    std::string bad = "{ not json }";
    req.set_content(sihd::util::ArrCharView(bad.data(), bad.size()));
    EXPECT_TRUE(req.content_as_json().is_discarded());
}

TEST_F(TestHttpRequest, test_routing_params)
{
    HttpRequest req("/api/users/42/edit");

    req.set_path_params({{"id", "42"}, {"action", "edit"}});
    EXPECT_EQ(req.path_param("id").value_or(""), "42");
    EXPECT_EQ(req.path_param("action").value_or(""), "edit");
    EXPECT_FALSE(req.path_param("missing").has_value());

    req.set_query_params({{"q", "hello"}, {"page", "2"}});
    EXPECT_EQ(req.query_param("q").value_or(""), "hello");
    EXPECT_EQ(req.query_param("page").value_or(""), "2");
    EXPECT_FALSE(req.query_param("other").has_value());
}

TEST_F(TestHttpRequest, test_cookies)
{
    HttpRequest req("/page");
    EXPECT_TRUE(req.cookies().empty());

    req.set_cookie("session", "abc123");
    req.set_cookie("user", "john");

    EXPECT_EQ(req.cookie("session").value_or(""), "abc123");
    EXPECT_EQ(req.cookie("user").value_or(""), "john");
    EXPECT_FALSE(req.cookie("other").has_value());
    EXPECT_EQ(req.cookies().size(), 2u);
}

TEST_F(TestHttpRequest, test_auth)
{
    HttpRequest req("/protected");
    EXPECT_FALSE(req.is_authenticated());

    req.set_auth_user("alice");
    EXPECT_EQ(req.auth_user(), "alice");
    EXPECT_TRUE(req.is_authenticated());

    req.set_auth_token("tok");
    EXPECT_EQ(req.auth_token(), "tok");
    EXPECT_EQ(req.auth_user(), "alice"); // both can be set simultaneously
    EXPECT_TRUE(req.is_authenticated());

    req.set_client_ip("127.0.0.1");
    EXPECT_EQ(req.client_ip(), "127.0.0.1");
}

} // namespace test
