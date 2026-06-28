#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Url.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestUrl: public ::testing::Test
{
    protected:
        TestUrl() { sihd::util::LoggerManager::stream(); }

        virtual ~TestUrl() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUrl, test_parse_basic)
{
    Url url("https://example.com/path");
    EXPECT_EQ(url.scheme, "https");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 0);
    EXPECT_EQ(url.path, "/path");
    EXPECT_TRUE(url.query_params.empty());
    EXPECT_TRUE(url.fragment.empty());
}

TEST_F(TestUrl, test_parse_with_port)
{
    Url url("http://localhost:8080/api/v1");
    EXPECT_EQ(url.scheme, "http");
    EXPECT_EQ(url.host, "localhost");
    EXPECT_EQ(url.port, 8080);
    EXPECT_EQ(url.path, "/api/v1");
}

TEST_F(TestUrl, test_parse_query_params)
{
    Url url("https://example.com/search?q=hello%20world&page=2");
    EXPECT_EQ(url.path, "/search");
    EXPECT_EQ(url.query_params.size(), 2u);
    EXPECT_EQ(url.query_params.at("q"), "hello world");
    EXPECT_EQ(url.query_params.at("page"), "2");
}

TEST_F(TestUrl, test_parse_fragment)
{
    Url url("https://example.com/docs#section-3");
    EXPECT_EQ(url.path, "/docs");
    EXPECT_EQ(url.fragment, "section-3");
}

TEST_F(TestUrl, test_parse_query_and_fragment)
{
    Url url("https://example.com/page?key=val#frag");
    EXPECT_EQ(url.path, "/page");
    EXPECT_EQ(url.query_params.at("key"), "val");
    EXPECT_EQ(url.fragment, "frag");
}

TEST_F(TestUrl, test_parse_no_path)
{
    Url url("https://example.com");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.path, "/");
}

TEST_F(TestUrl, test_parse_no_scheme)
{
    Url url("example.com/path");
    EXPECT_EQ(url.scheme, "");
    EXPECT_EQ(url.path, "example.com/path");
}

TEST_F(TestUrl, test_parse_websocket)
{
    Url url("wss://ws.example.com/chat");
    EXPECT_EQ(url.scheme, "wss");
    EXPECT_EQ(url.host, "ws.example.com");
    EXPECT_EQ(url.path, "/chat");

    Url url2("ws://localhost/events");
    EXPECT_EQ(url2.scheme, "ws");
}

TEST_F(TestUrl, test_to_string)
{
    Url url("https://example.com:8443/api?key=val#frag");
    auto str = url.encode();
    EXPECT_NE(str.find("https://example.com:8443/api"), std::string::npos);
    EXPECT_NE(str.find("key=val"), std::string::npos);
    EXPECT_NE(str.find("#frag"), std::string::npos);
}

TEST_F(TestUrl, test_roundtrip)
{
    Url url("https://example.com:9090/path?a=1&b=2#section");
    auto result = url.encode();
    Url reparsed(result);

    EXPECT_EQ(reparsed.scheme, url.scheme);
    EXPECT_EQ(reparsed.host, url.host);
    EXPECT_EQ(reparsed.port, url.port);
    EXPECT_EQ(reparsed.path, url.path);
    EXPECT_EQ(reparsed.query_params, url.query_params);
    EXPECT_EQ(reparsed.fragment, url.fragment);
}

TEST_F(TestUrl, test_url_str_encode)
{
    EXPECT_EQ(Url::str_encode("hello world"), "hello%20world");
    EXPECT_EQ(Url::str_encode("a=b&c"), "a%3Db%26c");
    EXPECT_EQ(Url::str_encode("simple"), "simple");
    EXPECT_EQ(Url::str_encode("foo@bar.com"), "foo%40bar.com");
}

TEST_F(TestUrl, test_url_str_decode)
{
    EXPECT_EQ(Url::str_decode("hello%20world"), "hello world");
    EXPECT_EQ(Url::str_decode("a%3Db%26c"), "a=b&c");
    EXPECT_EQ(Url::str_decode("simple"), "simple");
    EXPECT_EQ(Url::str_decode("foo%40bar.com"), "foo@bar.com");
    EXPECT_EQ(Url::str_decode("%2F%2F"), "//");
}

TEST_F(TestUrl, test_url_str_encode_decode_roundtrip)
{
    std::string special = "hello world!@#$%^&*()";
    EXPECT_EQ(Url::str_decode(Url::str_encode(special)), special);
}

TEST_F(TestUrl, test_decode_factory)
{
    Url url = Url::decode("https://example.com:8443/api?key=val#frag");
    EXPECT_EQ(url.scheme, "https");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.port, 8443);
    EXPECT_EQ(url.query_params.at("key"), "val");
    EXPECT_EQ(url.fragment, "frag");
}

TEST_F(TestUrl, test_encode_from_fields)
{
    Url url;
    url.scheme = "https";
    url.has_authority = true;
    url.host = "example.com";
    url.port = 8443;
    url.path = "/api";
    url.query_params["key"] = "val";
    url.fragment = "frag";
    EXPECT_EQ(url.encode(), "https://example.com:8443/api?key=val#frag");
}

TEST_F(TestUrl, test_parse_encoded_query_params)
{
    Url url("https://example.com/search?q=hello%20world&tag=c%2B%2B");
    EXPECT_EQ(url.query_params.at("q"), "hello world");
    EXPECT_EQ(url.query_params.at("tag"), "c++");
}

TEST_F(TestUrl, test_parse_userinfo)
{
    Url url("ftp://user:pass@ftp.example.com/files");
    EXPECT_EQ(url.scheme, "ftp");
    EXPECT_EQ(url.userinfo, "user:pass");
    EXPECT_EQ(url.host, "ftp.example.com");
    EXPECT_EQ(url.path, "/files");
}

TEST_F(TestUrl, test_parse_ipv6)
{
    Url url("http://[::1]:8080/path");
    EXPECT_EQ(url.host, "::1");
    EXPECT_EQ(url.port, 8080);
    EXPECT_EQ(url.path, "/path");

    Url url2("http://[::1]/path");
    EXPECT_EQ(url2.host, "::1");
    EXPECT_EQ(url2.port, 0);
}

TEST_F(TestUrl, test_parse_no_authority)
{
    Url url_news("news:fr.comp.infosystemes.www.auteurs");
    EXPECT_EQ(url_news.scheme, "news");
    EXPECT_FALSE(url_news.has_authority);
    EXPECT_TRUE(url_news.host.empty());
    EXPECT_EQ(url_news.path, "fr.comp.infosystemes.www.auteurs");

    Url url_mailto("mailto:quidam@exemple.com");
    EXPECT_EQ(url_mailto.scheme, "mailto");
    EXPECT_FALSE(url_mailto.has_authority);
    EXPECT_TRUE(url_mailto.host.empty());
    EXPECT_EQ(url_mailto.path, "quidam@exemple.com");
}

TEST_F(TestUrl, test_parse_file)
{
    Url url("file:///home/joe/Documents/cv.pdf");
    EXPECT_EQ(url.scheme, "file");
    EXPECT_TRUE(url.has_authority);
    EXPECT_TRUE(url.host.empty());
    EXPECT_EQ(url.path, "/home/joe/Documents/cv.pdf");
}

TEST_F(TestUrl, test_parse_gemini)
{
    Url url("gemini://geminiprotocol.net/docs/specification.gmi");
    EXPECT_EQ(url.scheme, "gemini");
    EXPECT_EQ(url.host, "geminiprotocol.net");
    EXPECT_EQ(url.path, "/docs/specification.gmi");
}

TEST_F(TestUrl, test_to_string_no_authority)
{
    Url url("mailto:quidam@exemple.com");
    EXPECT_EQ(url.encode(), "mailto:quidam@exemple.com");

    Url url2("news:fr.comp.infosystemes.www.auteurs");
    EXPECT_EQ(url2.encode(), "news:fr.comp.infosystemes.www.auteurs");
}

TEST_F(TestUrl, test_to_string_file)
{
    Url url("file:///home/joe/Documents/cv.pdf");
    EXPECT_EQ(url.encode(), "file:///home/joe/Documents/cv.pdf");
}

TEST_F(TestUrl, test_to_string_userinfo)
{
    Url url("ftp://user:pass@ftp.example.com/files");
    EXPECT_EQ(url.encode(), "ftp://user:pass@ftp.example.com/files");
}

} // namespace test
