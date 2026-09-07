#include <gtest/gtest.h>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/screenshot.hpp>

namespace test
{
using namespace sihd::sys;

class TestScreenshotBackend: public ::testing::Test
{
    protected:
        TestScreenshotBackend() = default;
        virtual ~TestScreenshotBackend() = default;
        virtual void SetUp()
        {
            if constexpr (!screenshot::supported)
            {
                GTEST_SKIP() << "no screenshot backend compiled in";
            }
        }
        virtual void TearDown() {}
};

TEST_F(TestScreenshotBackend, test_take_screen)
{
    Bitmap bm;
    // Success depends on the live session: no compositor means no capture.
    screenshot::take_screen(bm);
    if (!bm.empty())
    {
        EXPECT_GT(bm.width(), 0u);
        EXPECT_GT(bm.height(), 0u);
    }
}

TEST_F(TestScreenshotBackend, test_take_focused)
{
    Bitmap bm;
    // Falls back to a full screen capture where no focused window exists.
    screenshot::take_focused(bm);
    if (!bm.empty())
    {
        EXPECT_GT(bm.width(), 0u);
        EXPECT_GT(bm.height(), 0u);
    }
}

TEST_F(TestScreenshotBackend, test_take_under_cursor)
{
    Bitmap bm;
    // Unsupported on wayland: falls back to a full screen capture.
    screenshot::take_under_cursor(bm);
    if (!bm.empty())
    {
        EXPECT_GT(bm.width(), 0u);
        EXPECT_GT(bm.height(), 0u);
    }
}

} // namespace test
