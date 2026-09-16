#include <gtest/gtest.h>

#include <sihd/http/Multipart.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::http;
using namespace std::literals;

class TestMultipart: public ::testing::Test
{
    protected:
        TestMultipart() { sihd::util::LoggerManager::stream(); }
        virtual ~TestMultipart() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestMultipart, test_boundary)
{
    auto boundary = Multipart::boundary("multipart/form-data; boundary=xyz");
    ASSERT_TRUE(boundary.has_value());
    EXPECT_EQ(*boundary, "xyz");

    boundary = Multipart::boundary("multipart/form-data; boundary=\"quoted boundary\"");
    ASSERT_TRUE(boundary.has_value());
    EXPECT_EQ(*boundary, "quoted boundary");

    auto multipart = Multipart::parse("--quoted boundary\r\n\r\nquoted\r\n--quoted boundary--",
                                      "multipart/form-data; boundary=\"quoted boundary\"");
    ASSERT_TRUE(multipart.has_value());
    ASSERT_EQ(multipart->parts().size(), 1u);
    EXPECT_EQ(multipart->parts()[0].data, "quoted");

    EXPECT_FALSE(Multipart::boundary("multipart/form-data").has_value());
    EXPECT_FALSE(Multipart::boundary("multipart/form-datax; boundary=xyz").has_value());
    EXPECT_FALSE(Multipart::boundary("application/x-www-form-urlencoded; boundary=xyz").has_value());
}

TEST_F(TestMultipart, test_parse)
{
    const std::string body = "--boundary\r\n"
                             "Content-Disposition: form-data; name=\"username\"\r\n"
                             "\r\n"
                             "testuser\r\n"
                             "--boundary\r\n"
                             "Content-Disposition: form-data; name=\"avatar\"; filename=\"a.txt\"\r\n"
                             "Content-Type: text/plain\r\n"
                             "\r\n"
                             "hello file\r\n"
                             "--boundary--\r\n";

    auto multipart = Multipart::parse(body, "multipart/form-data; boundary=boundary");
    ASSERT_TRUE(multipart.has_value());
    ASSERT_EQ(multipart->parts().size(), 2u);

    EXPECT_EQ(multipart->parts()[0].name, "username");
    EXPECT_EQ(multipart->parts()[0].filename, "");
    EXPECT_EQ(multipart->parts()[0].data, "testuser");
    EXPECT_EQ(multipart->parts()[1].name, "avatar");
    EXPECT_EQ(multipart->parts()[1].filename, "a.txt");
    EXPECT_EQ(multipart->parts()[1].content_type, "text/plain");
    EXPECT_EQ(multipart->parts()[1].data, "hello file");

    auto username = multipart->value("username");
    ASSERT_TRUE(username.has_value());
    EXPECT_EQ(*username, "testuser");
    EXPECT_FALSE(multipart->value("missing").has_value());

    const Multipart::Part *file = multipart->file("avatar");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->filename, "a.txt");
    EXPECT_EQ(file->data, "hello file");
    EXPECT_EQ(multipart->file("username"), nullptr);

    // no part at all
    auto empty = Multipart::parse("--boundary--\r\n", "multipart/form-data; boundary=boundary");
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->parts().empty());

    // duplicated name: the first part wins
    auto duplicated = Multipart::parse("--b\r\n"
                                       "Content-Disposition: form-data; name=\"field\"\r\n"
                                       "\r\n"
                                       "first\r\n"
                                       "--b\r\n"
                                       "Content-Disposition: form-data; name=\"field\"\r\n"
                                       "\r\n"
                                       "second\r\n"
                                       "--b--",
                                       "multipart/form-data; boundary=b");
    ASSERT_TRUE(duplicated.has_value());
    ASSERT_EQ(duplicated->parts().size(), 2u);
    EXPECT_EQ(duplicated->value("field").value_or(""), "first");
}

TEST_F(TestMultipart, test_build)
{
    Multipart multipart;
    EXPECT_TRUE(multipart.empty());

    multipart.add_field("username", "sihd");
    multipart.add_field("json", "{}", "application/json");
    multipart.add_file("document", "/tmp/doc.txt", "doc.txt", "text/plain");

    ASSERT_EQ(multipart.parts().size(), 3u);
    EXPECT_FALSE(multipart.empty());
    EXPECT_EQ(multipart.value("username").value_or(""), "sihd");
    EXPECT_EQ(multipart.parts()[1].content_type, "application/json");
    EXPECT_FALSE(multipart.parts()[1].is_file());

    const Multipart::Part *file = multipart.file("document");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->path, "/tmp/doc.txt");
    EXPECT_EQ(file->filename, "doc.txt");
    EXPECT_EQ(file->content_type, "text/plain");
    EXPECT_TRUE(file->is_file());
    EXPECT_EQ(multipart.file("username"), nullptr);

    // the navigator copies its payload around: parts must survive a copy
    Multipart copy = multipart;
    EXPECT_EQ(copy.value("username").value_or(""), "sihd");
    EXPECT_EQ(copy.file("document")->path, "/tmp/doc.txt");

    multipart.clear();
    EXPECT_TRUE(multipart.empty());
}

TEST_F(TestMultipart, test_parse_boundary_like_data)
{
    // a boundary-like sequence that does not end its line is part data
    const std::string body = "--b\r\n"
                             "Content-Disposition: form-data; name=\"doc\"\r\n"
                             "\r\n"
                             "before\r\n--bX after\r\n"
                             "--b \r\n"
                             "Content-Disposition: form-data; name=\"next\"\r\n"
                             "\r\n"
                             "second\r\n"
                             "--b--";

    auto multipart = Multipart::parse(body, "multipart/form-data; boundary=b");
    ASSERT_TRUE(multipart.has_value());
    ASSERT_EQ(multipart->parts().size(), 2u);
    EXPECT_EQ(multipart->parts()[0].data, "before\r\n--bX after");
    EXPECT_EQ(multipart->parts()[1].data, "second");
}

TEST_F(TestMultipart, test_parse_binary_and_preamble)
{
    std::string body = "this preamble is ignored\r\n--b\r\n\r\n";
    body += std::string("\x00\x01\x02\xff", 4);
    body += "\r\n--b--";

    auto multipart = Multipart::parse(body, "multipart/form-data; boundary=b");
    ASSERT_TRUE(multipart.has_value());
    ASSERT_EQ(multipart->parts().size(), 1u);
    EXPECT_EQ(multipart->parts()[0].name, "");
    EXPECT_EQ(multipart->parts()[0].data, "\x00\x01\x02\xff"sv);
}

TEST_F(TestMultipart, test_parse_disposition_variants)
{
    const std::string body = "--b\r\n"
                             "Content-Disposition: form-data; name=doc; filename=\"a=b.txt\"\r\n"
                             "Content-Type:text/plain\r\n"
                             "\r\n"
                             "content\r\n"
                             "--b--";

    auto multipart = Multipart::parse(body, "multipart/form-data; boundary=b");
    ASSERT_TRUE(multipart.has_value());
    ASSERT_EQ(multipart->parts().size(), 1u);
    EXPECT_EQ(multipart->parts()[0].name, "doc");
    EXPECT_EQ(multipart->parts()[0].filename, "a=b.txt");
    EXPECT_EQ(multipart->parts()[0].content_type, "text/plain");
}

TEST_F(TestMultipart, test_parse_malformed)
{
    const std::string valid = "--b\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\nvalue\r\n--b--";

    EXPECT_FALSE(Multipart::parse("", "multipart/form-data; boundary=b").has_value());
    EXPECT_FALSE(Multipart::parse(valid, "multipart/form-data").has_value());
    EXPECT_FALSE(Multipart::parse(valid, "multipart/form-data; boundary=other").has_value());
    // no closing delimiter
    EXPECT_FALSE(Multipart::parse(valid.substr(0, valid.size() - 2), "multipart/form-data; boundary=b").has_value());
    // header line without colon
    EXPECT_FALSE(
        Multipart::parse("--b\r\nbroken header\r\n\r\nvalue\r\n--b--", "multipart/form-data; boundary=b").has_value());
    EXPECT_FALSE(Multipart::parse("--b", "multipart/form-data; boundary=b").has_value());
}

TEST_F(TestMultipart, test_first_delimiter_must_start_a_line)
{
    // a delimiter inside the preamble line does not start the parts
    EXPECT_FALSE(Multipart::parse("xx--b\r\nxx--b--", "multipart/form-data; boundary=b").has_value());

    // one at the very start still does
    auto at_start = Multipart::parse("--b\r\n\r\nvalue\r\n--b--", "multipart/form-data; boundary=b");
    ASSERT_TRUE(at_start.has_value());
    ASSERT_EQ(at_start->parts().size(), 1u);
}

TEST_F(TestMultipart, test_is_content_type)
{
    EXPECT_TRUE(Multipart::is_content_type("multipart/form-data; boundary=b"));
    EXPECT_TRUE(Multipart::is_content_type("Multipart/Form-Data;boundary=b"));
    EXPECT_TRUE(Multipart::is_content_type("multipart/form-data "));
    // media type matching stops at the parameters
    EXPECT_FALSE(Multipart::is_content_type("multipart/form-datax; boundary=b"));
    EXPECT_FALSE(Multipart::is_content_type("text/plain"));
    EXPECT_FALSE(Multipart::is_content_type(""));
}

TEST_F(TestMultipart, test_add_rejects_control_characters)
{
    Multipart multipart;

    multipart.add_field("name\r\nInjected-Header: x", "value");
    multipart.add_field("ok", "value");
    ASSERT_EQ(multipart.parts().size(), 1u);
    EXPECT_EQ(multipart.parts()[0].name, "ok");

    multipart.add_file("doc", "/tmp/f.txt", "a\"b.txt");
    EXPECT_EQ(multipart.parts().size(), 1u);

    // a file added without filename stays retrievable
    multipart.add_file("doc", "/tmp/f.txt");
    ASSERT_EQ(multipart.parts().size(), 2u);
    EXPECT_NE(multipart.file("doc"), nullptr);
}

TEST_F(TestMultipart, test_parse_bounds_part_count)
{
    std::string body;
    for (int i = 0; i < 10001; ++i)
        body += "--b\r\n\r\nx\r\n";
    body += "--b--";

    EXPECT_FALSE(Multipart::parse(body, "multipart/form-data; boundary=b").has_value());
}

} // namespace test
