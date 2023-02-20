#include <gtest/gtest.h>
#include <sihd/util/LoadingBar.hpp>

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

    LoadingBar bar(width, 100);

    bar.set_percent_pos_right();
    bar.add_progress(0);
    for (size_t i = 0; i < 100; ++i)
    {
        ASSERT_TRUE(bar.add_progress(1));
    }
    std::cout << std::endl;
}
} // namespace test
