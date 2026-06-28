#include <gtest/gtest.h>
#include <sihd/json/Json.hpp>

#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::http;

class TestHttpResponse: public ::testing::Test
{
    protected:
        TestHttpResponse() { sihd::util::LoggerManager::stream(); }
        virtual ~TestHttpResponse() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestHttpResponse, test_content_types)
{
    // plain: sets text/plain mime and the body
    HttpResponse plain;
    EXPECT_TRUE(plain.set_plain_content("hello"));
    EXPECT_EQ(plain.content().cpp_str(), "hello");
    EXPECT_NE(plain.http_header().content_type().find("text/plain"), std::string::npos);

    // json: serializes and sets application/json
    HttpResponse json;
    EXPECT_TRUE(json.set_json_content({{"k", "v"}, {"n", 1}}));
    auto parsed = sihd::json::Json::parse(json.content().cpp_str());
    EXPECT_EQ(parsed["k"].get<std::string>(), "v");
    EXPECT_NE(json.http_header().content_type().find("application/json"), std::string::npos);

    // byte: sets octet-stream and preserves raw bytes
    HttpResponse bytes;
    const uint8_t raw[] = {0x00, 0xFF, 0x42};
    EXPECT_TRUE(bytes.set_byte_content(sihd::util::ArrByteView(raw, 3)));
    EXPECT_EQ(bytes.content().size(), 3u);
    EXPECT_NE(bytes.http_header().content_type().find("octet-stream"), std::string::npos);
}

TEST_F(TestHttpResponse, test_content_type_set_once)
{
    // mime type auto-detection should not overwrite an already-set type
    HttpResponse resp;
    EXPECT_TRUE(resp.set_plain_content("first"));
    std::string ct = std::string(resp.http_header().content_type());
    EXPECT_FALSE(ct.empty());
    EXPECT_TRUE(resp.set_plain_content("second"));
    EXPECT_EQ(resp.http_header().content_type(), ct);

    // explicit type on a fresh response is preserved
    HttpResponse resp2;
    resp2.set_content_type("application/xml");
    EXPECT_NE(resp2.http_header().content_type().find("application/xml"), std::string::npos);
    // auto-detection does not override the explicitly set type
    resp2.set_plain_content("data");
    EXPECT_NE(resp2.http_header().content_type().find("application/xml"), std::string::npos);
}

TEST_F(TestHttpResponse, test_cookies)
{
    HttpResponse resp;
    EXPECT_TRUE(resp.set_cookie_headers().empty());

    resp.set_cookie("session", "abc");
    resp.set_cookie("token", "xyz", "HttpOnly; Secure; Path=/");
    resp.set_cookie("uid", "1");

    const auto & cookies = resp.set_cookie_headers();
    ASSERT_EQ(cookies.size(), 3u);

    auto has = [&](std::string_view s) {
        for (const auto & c : cookies)
            if (c == s)
                return true;
        return false;
    };
    EXPECT_TRUE(has("session=abc"));
    EXPECT_TRUE(has("token=xyz; HttpOnly; Secure; Path=/"));
    EXPECT_TRUE(has("uid=1"));
}

TEST_F(TestHttpResponse, test_streaming)
{
    HttpResponse resp;
    EXPECT_FALSE(resp.is_streaming());

    int calls = 0;
    resp.set_stream_provider([&](sihd::util::ArrByte & chunk) -> bool {
        std::string s = "chunk" + std::to_string(calls++);
        chunk.resize(s.size());
        chunk.copy_from_bytes(s.data(), s.size());
        return calls < 3;
    });
    EXPECT_TRUE(resp.is_streaming());

    // consume all chunks and verify content + termination
    std::string body;
    sihd::util::ArrByte buf;
    bool more = true;
    while (more)
    {
        more = resp.stream_provider()(buf);
        body += buf.cpp_str();
    }
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(body, "chunk0chunk1chunk2");
}

TEST_F(TestHttpResponse, test_from_string)
{
    std::string raw = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "\r\n"
                      "hello";

    auto resp = HttpResponse::from_string(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "hello");
    EXPECT_NE(resp->http_header().content_type().find("text/plain"), std::string::npos);
}

TEST_F(TestHttpResponse, test_from_string_no_body)
{
    std::string raw = "HTTP/1.1 204 No Content\r\n\r\n";

    auto resp = HttpResponse::from_string(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::NoContent);
    EXPECT_EQ(resp->content().size(), 0u);
}

TEST_F(TestHttpResponse, test_from_string_with_cookies)
{
    std::string raw = "HTTP/1.1 200 OK\r\n"
                      "Set-Cookie: session=abc\r\n"
                      "Set-Cookie: token=xyz; HttpOnly\r\n"
                      "\r\n";

    auto resp = HttpResponse::from_string(raw);
    ASSERT_TRUE(resp.has_value());
    ASSERT_EQ(resp->set_cookie_headers().size(), 2u);
    EXPECT_EQ(resp->set_cookie_headers()[0], "session=abc");
    EXPECT_EQ(resp->set_cookie_headers()[1], "token=xyz; HttpOnly");
}

TEST_F(TestHttpResponse, test_from_string_http10)
{
    std::string raw = "HTTP/1.0 301 Moved Permanently\r\n\r\n";
    auto resp = HttpResponse::from_string(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::MovedPermanently);
}

TEST_F(TestHttpResponse, test_from_string_malformed)
{
    EXPECT_FALSE(HttpResponse::from_string("garbage").has_value());
    EXPECT_FALSE(HttpResponse::from_string("HTTP/2.0 200 OK\r\n\r\n").has_value());
    EXPECT_FALSE(HttpResponse::from_string("HTTP/1.1\r\n\r\n").has_value());
}

TEST_F(TestHttpResponse, test_to_string)
{
    HttpResponse resp;
    resp.set_status(HttpStatus::NotFound);
    resp.set_plain_content("not found");
    resp.set_cookie("id", "42");

    std::string s = resp.to_string();
    EXPECT_NE(s.find("HTTP/1.1 404 Not Found\r\n"), std::string::npos);
    EXPECT_NE(s.find("Set-Cookie: id=42\r\n"), std::string::npos);
    EXPECT_NE(s.find("\r\n\r\nnot found"), std::string::npos);
}

TEST_F(TestHttpResponse, test_roundtrip)
{
    HttpResponse original;
    original.set_status(HttpStatus::Created);
    original.http_header().set_header("x-request-id", "abc-123");
    original.set_plain_content("created");

    std::string serialized = original.to_string();
    auto parsed = HttpResponse::from_string(serialized);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->status(), HttpStatus::Created);
    EXPECT_EQ(parsed->content().cpp_str(), "created");
    EXPECT_EQ(parsed->http_header().find("x-request-id"), "abc-123");
}

TEST_F(TestHttpResponse, test_status)
{
    HttpResponse resp;
    EXPECT_EQ(resp.status(), HttpStatus::Ok);
    resp.set_status(HttpStatus::NotFound);
    EXPECT_EQ(resp.status(), HttpStatus::NotFound);
    resp.set_status(HttpStatus::Created);
    EXPECT_EQ(resp.status(), HttpStatus::Created);
}

} // namespace test
