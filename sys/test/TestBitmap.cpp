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

TEST_F(TestBitmap, test_bitmap_pixel_set_get)
{
    Bitmap bmp(10, 10, 32);

    Pixel red = Pixel::rgb(255, 0, 0);
    bmp.set(5, 5, red);

    Pixel got = bmp.get(5, 5);
    EXPECT_EQ(got.red, 255);
    EXPECT_EQ(got.green, 0);
    EXPECT_EQ(got.blue, 0);
}

TEST_F(TestBitmap, test_bitmap_fill)
{
    Bitmap bmp(3, 3, 32);

    Pixel green = Pixel::rgb(0, 255, 0);
    bmp.fill(green);

    for (size_t y = 0; y < 3; ++y)
    {
        for (size_t x = 0; x < 3; ++x)
        {
            Pixel p = bmp.get(x, y);
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
    Pixel blue = Pixel::rgb(0, 0, 255);
    bmp.fill(blue);

    auto data = bmp.to_bmp_data();
    EXPECT_FALSE(data.empty());

    Bitmap bmp2;
    EXPECT_TRUE(bmp2.read_bmp_data(data));
    EXPECT_EQ(bmp2.width(), 4u);
    EXPECT_EQ(bmp2.height(), 4u);

    Pixel p = bmp2.get(0, 0);
    EXPECT_EQ(p.blue, 255);
    EXPECT_EQ(p.red, 0);
}

TEST_F(TestBitmap, test_bitmap_clear)
{
    Bitmap bmp(4, 4, 32);
    EXPECT_FALSE(bmp.empty());
    bmp.clear();
    EXPECT_TRUE(bmp.empty());
}

} // namespace test
