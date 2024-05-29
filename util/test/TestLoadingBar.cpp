#include <gtest/gtest.h>

#include <sihd/util/LoadingBar.hpp>
#include <sihd/util/time.hpp>

namespace test
{
using namespace sihd::util;
class TestLoadingBar: public ::testing::Test
{
    protected:
        TestLoadingBar() {}

        virtual ~TestLoadingBar() {}

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestLoadingBar, test_loadingbar)
{
    constexpr size_t width = 20;
    constexpr size_t total = 100;

    LoadingBar bar(width, total);

    bar.set_percent_pos_right();
    for (size_t i = 0; i < 100; ++i)
    {
        bar.add_progress(1);
        ASSERT_TRUE(bar.print());
    }
    std::cout << std::endl;
}
} // namespace test
