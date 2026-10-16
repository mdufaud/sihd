#include <algorithm>
#include <limits>

#include <gtest/gtest.h>

#include <sihd/sys/Bitmap.hpp>

namespace test
{
using namespace sihd::sys;

class TestBitmap: public ::testing::Test
{
    protected:
        TestBitmap() = default;
        virtual ~TestBitmap() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestBitmap, test_bitmap_create_32bit)
{
    Bitmap bmp(4, 4, 32);

    EXPECT_EQ(bmp.width(), 4u);
    EXPECT_EQ(bmp.height(), 4u);
    EXPECT_EQ(bmp.byte_per_pixel(), 4u);
    EXPECT_FALSE(bmp.empty());
}

TEST_F(TestBitmap, test_bitmap_create_24bit)
{
    Bitmap bmp(4, 4, 24);

    EXPECT_EQ(bmp.width(), 4u);
    EXPECT_EQ(bmp.height(), 4u);
    EXPECT_EQ(bmp.byte_per_pixel(), 3u);
    EXPECT_FALSE(bmp.empty());
}

TEST_F(TestBitmap, test_bitmap_overflow)
{
    Bitmap bitmap;
    EXPECT_THROW(bitmap.create(std::numeric_limits<size_t>::max(), 2, 32), std::length_error);
    EXPECT_TRUE(bitmap.empty());
}

TEST_F(TestBitmap, test_bitmap_bgr_encoding)
{
    Bitmap bitmap(1, 1, 24);
    bitmap.fill(Color::rgb(1, 2, 3));
    const auto bytes = bitmap.to_bmp_data();
    ASSERT_EQ(bytes.size(), 58u);
    EXPECT_EQ(bytes[2], 58);
    EXPECT_EQ(bytes[3], 0);
    EXPECT_EQ(bytes[18], 1);
    EXPECT_EQ(bytes[19], 0);
    EXPECT_EQ(bytes[54], 3);
    EXPECT_EQ(bytes[55], 2);
    EXPECT_EQ(bytes[56], 1);
    EXPECT_EQ(bytes[57], 0);
}

TEST_F(TestBitmap, test_bitmap_pixel_set_get)
{
    Bitmap bmp(10, 10, 32);

    Color red = Color::rgb(255, 0, 0);
    bmp.set(5, 5, red);

    Color got = bmp.get(5, 5);
    EXPECT_EQ(got.red, 255);
    EXPECT_EQ(got.green, 0);
    EXPECT_EQ(got.blue, 0);
}

TEST_F(TestBitmap, test_bitmap_fill)
{
    Bitmap bmp(3, 3, 32);

    Color green = Color::rgb(0, 255, 0);
    bmp.fill(green);

    for (size_t y = 0; y < 3; ++y)
    {
        for (size_t x = 0; x < 3; ++x)
        {
            Color p = bmp.get(x, y);
            EXPECT_EQ(p.green, 255);
            EXPECT_EQ(p.red, 0);
            EXPECT_EQ(p.blue, 0);
        }
    }
}

TEST_F(TestBitmap, test_bitmap_is_accessible)
{
    Bitmap bmp(5, 5, 32);

    EXPECT_TRUE(bmp.is_accessible(0, 0));
    EXPECT_TRUE(bmp.is_accessible(4, 4));
    EXPECT_FALSE(bmp.is_accessible(5, 5));
}

TEST_F(TestBitmap, test_bitmap_bmp_roundtrip)
{
    Bitmap bmp(4, 4, 32);
    Color blue = Color::rgb(0, 0, 255);
    bmp.fill(blue);

    auto data = bmp.to_bmp_data();
    EXPECT_FALSE(data.empty());

    Bitmap bmp2;
    EXPECT_TRUE(bmp2.read_bmp_data(data));
    EXPECT_EQ(bmp2.width(), 4u);
    EXPECT_EQ(bmp2.height(), 4u);

    Color p = bmp2.get(0, 0);
    EXPECT_EQ(p.blue, 255);
    EXPECT_EQ(p.red, 0);
}

TEST_F(TestBitmap, test_bitmap_bitfield_headers)
{
    Color color = Color::rgb(17, 34, 51);
    color.alpha = 68;
    Bitmap source(1, 1, 32);
    source.fill(color);
    for (size_t header_size : {40, 108, 124})
    {
        auto data = source.to_bmp_data();
        const size_t extension = std::max(header_size - 40, size_t(12));
        data.insert(data.begin() + 54, extension, 0);
        data[2] = data.size();
        data[10] = 54 + extension;
        data[14] = header_size;
        data[30] = 3;
        data[56] = data[59] = data[62] = 0xff;
        if (header_size >= 108)
            data[69] = 0xff;
        Bitmap decoded;
        ASSERT_TRUE(decoded.read_bmp_data(data));
        const Color pixel = decoded.get(0, 0);
        EXPECT_EQ(pixel.red, 17);
        EXPECT_EQ(pixel.green, 34);
        EXPECT_EQ(pixel.blue, 51);
        EXPECT_EQ(pixel.alpha, header_size == 40 ? 255 : 68);
    }
}

TEST_F(TestBitmap, test_bitmap_rejects_oversized_header)
{
    auto data = Bitmap(1, 1, 32).to_bmp_data();
    for (size_t i = 14; i < 18; ++i)
        data[i] = 0xff;
    Bitmap decoded;
    EXPECT_FALSE(decoded.read_bmp_data(data));
}

TEST_F(TestBitmap, test_bitmap_rejects_overflowing_palette)
{
    auto data = Bitmap(1, 1, 32).to_bmp_data();
    data[28] = 8;
    data[49] = 0x40;
    data[54] = 0xff;
    Bitmap decoded;
    EXPECT_FALSE(decoded.read_bmp_data(data));
}

TEST_F(TestBitmap, test_bitmap_rejects_overflowing_row)
{
    auto data = Bitmap(1, 1, 32).to_bmp_data();
    data[18] = 0;
    data[21] = 0x40;
    Bitmap decoded;
    EXPECT_FALSE(decoded.read_bmp_data(data));
}

TEST_F(TestBitmap, test_bitmap_clear)
{
    Bitmap bmp(4, 4, 32);
    EXPECT_FALSE(bmp.empty());
    bmp.clear();
    EXPECT_TRUE(bmp.empty());
}

} // namespace test
