#include <gtest/gtest.h>

#include <sihd/util/mime.hpp>

namespace test
{
using namespace sihd::util;

TEST(TestMime, test_mime_compatible)
{
    EXPECT_TRUE(mime::compatible(mime::html, mime::html));
    EXPECT_TRUE(mime::compatible("text/plain;charset=utf-8", mime::plain_text));
    EXPECT_TRUE(mime::compatible("TEXT/HTML", mime::html));
    EXPECT_TRUE(mime::compatible("Text/Plain", "text/PLAIN"));
    EXPECT_TRUE(mime::compatible(mime::html, ""));
    EXPECT_TRUE(mime::compatible("", mime::bmp_image));
    EXPECT_FALSE(mime::compatible(mime::html, mime::plain_text));
    EXPECT_FALSE(mime::compatible(mime::html, "image/html"));
}

TEST(TestMime, test_mime_match_offered)
{
    const std::vector<std::string> offered = {"image/bmp", "text/plain;charset=utf-8"};

    auto match = mime::match_offered(offered, mime::plain_text);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(*match, "text/plain;charset=utf-8");

    EXPECT_EQ(mime::match_offered(offered, "").value_or("nope"), "image/bmp");
    EXPECT_FALSE(mime::match_offered({"image/bmp"}, mime::html).has_value());
    EXPECT_FALSE(mime::match_offered({}, mime::html).has_value());
}

TEST(TestMime, test_mime_best_match)
{
    const std::vector<std::string> offered = {"text/plain"};
    const std::vector<std::string_view> wanted = {mime::bmp_image, mime::html, mime::plain_text};

    auto match = mime::best_match(offered, wanted);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(*match, mime::plain_text);

    EXPECT_FALSE(mime::best_match({"image/bmp"}, {mime::html}).has_value());
    EXPECT_FALSE(mime::best_match({}, {mime::html}).has_value());
}

} // namespace test
