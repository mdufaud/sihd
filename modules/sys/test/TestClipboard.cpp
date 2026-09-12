#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/clipboard.hpp>

#include "test_helper.hpp"

#if !defined(__SIHD_WINDOWS__)
# include "../src/linux/internal/desktop_env.hpp"
#endif

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

namespace mime = sihd::util::mime;

class TestClipboardBackend: public ::testing::Test
{
    protected:
        TestClipboardBackend() = default;
        virtual ~TestClipboardBackend() = default;
        virtual void SetUp()
        {
            if constexpr (!clipboard::supported)
            {
                GTEST_SKIP() << "no clipboard backend compiled in";
            }
        }
        virtual void TearDown() {}
};

TEST(TestClipboardMime, test_best_match)
{
    // Wanted priority wins; parameters are ignored.
    EXPECT_EQ(mime::best_match({"text/plain;charset=utf-8", "text/plain"}, {mime::utf8_text, mime::plain_text}),
              mime::utf8_text);
    EXPECT_EQ(mime::best_match({"text/plain", "text/plain;charset=utf-8"}, {mime::utf8_text, mime::plain_text}),
              mime::utf8_text);
    EXPECT_EQ(mime::best_match({"text/plain"}, {mime::utf8_text, mime::plain_text}), mime::utf8_text);
    EXPECT_EQ(mime::best_match({"text/html", "text/plain"}, {mime::utf8_text, mime::plain_text}), mime::utf8_text);

    // Nothing wanted is offered or compatible.
    EXPECT_FALSE(mime::best_match({}, {mime::utf8_text}).has_value());
    EXPECT_FALSE(mime::best_match({"application/octet-stream"}, {mime::utf8_text}).has_value());
    EXPECT_FALSE(mime::best_match({"text/html"}, {mime::utf8_text, mime::plain_text}).has_value());

    // match_offered returns the offered entry.
    EXPECT_EQ(mime::match_offered({"text/plain;charset=utf-8", "text/plain"}, mime::plain_text),
              std::string_view("text/plain;charset=utf-8"));
    EXPECT_FALSE(mime::match_offered({"text/html"}, mime::uri_list).has_value());
}

TEST(TestClipboardConvert, test_to_text)
{
    clipboard::RawContent raw {std::string(mime::utf8_text), {'h', 'e', 'l', 'l', 'o'}};
    auto text = clipboard::to_text(raw);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, "hello");

    // Only text mime types convert.
    clipboard::RawContent html {std::string(mime::html), {'<', 'b', '>'}};
    EXPECT_FALSE(clipboard::to_text(html).has_value());
}

TEST(TestClipboardConvert, test_to_image)
{
    Bitmap bm(4, 3);
    bm.fill(Color::rgb(1, 2, 3));

    clipboard::RawContent raw {std::string(mime::bmp_image), bm.to_bmp_data()};
    auto decoded = clipboard::to_image(raw);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width(), 4u);
    EXPECT_EQ(decoded->height(), 3u);
    EXPECT_EQ(decoded->data(), bm.data());

    // Only image/bmp converts, and broken bytes do not.
    clipboard::RawContent wrong_mime {std::string(mime::uri_list), {}};
    EXPECT_FALSE(clipboard::to_image(wrong_mime).has_value());
    clipboard::RawContent garbage {std::string(mime::bmp_image), {1, 2, 3}};
    EXPECT_FALSE(clipboard::to_image(garbage).has_value());
}

TEST(TestClipboardConvert, test_to_image_indexed_bmp)
{
    std::vector<uint8_t> bmp(62 + 8, 0);
    auto write_u32 = [&bmp](size_t offset, uint32_t value) {
        memcpy(bmp.data() + offset, &value, sizeof(value));
    };
    auto write_u16 = [&bmp](size_t offset, uint16_t value) {
        memcpy(bmp.data() + offset, &value, sizeof(value));
    };
    bmp[0] = 'B';
    bmp[1] = 'M';
    write_u32(10, 62); // pixel array offset: file + info headers + color table
    write_u32(14, 40); // info header size
    write_u32(18, 2);  // width
    write_u32(22, 2);  // height
    write_u16(26, 1);  // planes
    write_u16(28, 8);  // bits per pixel
    write_u32(30, 0);  // BI_RGB
    write_u32(46, 2);  // nb_colors
    // color table (stored blue, green, red): entry0 = red, entry1 = blue
    bmp[54] = 0;
    bmp[55] = 0;
    bmp[56] = 255;
    bmp[57] = 0;
    bmp[58] = 255;
    bmp[59] = 0;
    bmp[60] = 0;
    bmp[61] = 0;
    // 2-pixel rows are padded to a 4-byte stride; rows are bottom-up.
    bmp[62] = 1;
    bmp[63] = 0;
    bmp[66] = 0;
    bmp[67] = 1;

    clipboard::RawContent indexed {std::string(mime::bmp_image), bmp};
    auto decoded = clipboard::to_image(indexed);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width(), 2u);
    EXPECT_EQ(decoded->height(), 2u);
    EXPECT_EQ(decoded->get(0, 0).red, 255);
    EXPECT_EQ(decoded->get(1, 0).blue, 255);
    EXPECT_EQ(decoded->get(0, 1).blue, 255);
    EXPECT_EQ(decoded->get(1, 1).red, 255);
}

#if !defined(__SIHD_WINDOWS__)

TEST(TestSessionDetection, test_wayland_session_env)
{
    {
        ScopedEnv display("WAYLAND_DISPLAY", "wayland-0");
        EXPECT_TRUE(internal::wayland_session());
    }
    {
        ScopedEnv socket("WAYLAND_SOCKET", "3");
        EXPECT_TRUE(internal::wayland_session());
    }
    {
        ScopedEnv display("WAYLAND_DISPLAY", std::nullopt);
        ScopedEnv socket("WAYLAND_SOCKET", std::nullopt);
        ScopedEnv type("XDG_SESSION_TYPE", "wayland");
        EXPECT_TRUE(internal::wayland_session());
    }
    {
        ScopedEnv display("WAYLAND_DISPLAY", std::nullopt);
        ScopedEnv socket("WAYLAND_SOCKET", std::nullopt);
        ScopedEnv type("XDG_SESSION_TYPE", "x11");
        EXPECT_FALSE(internal::wayland_session());
    }
}

#endif

TEST_F(TestClipboardBackend, test_get_raw)
{
    // Must complete on its own: the result depends on the live clipboard.
    auto contents = clipboard::get_raw();
    for (const clipboard::RawContent & raw : contents)
    {
        EXPECT_FALSE(raw.mime.empty());
    }

    // The filtered get only returns entries among the wanted mime types.
    auto wanted = clipboard::get_raw({mime::utf8_text, mime::bmp_image});
    for (const clipboard::RawContent & raw : wanted)
    {
        EXPECT_TRUE(raw.mime == mime::utf8_text || raw.mime == mime::bmp_image);
    }
}

TEST_F(TestClipboardBackend, test_set_raw_empty)
{
    // An empty set must be rejected without touching the clipboard.
    EXPECT_FALSE(clipboard::set_raw({}));
}

} // namespace

} // namespace test
