#include <atomic>
#include <future>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/util/Timer.hpp>

namespace test
{
using namespace sihd::util;

class TestTimer: public ::testing::Test
{
    protected:
        TestTimer() = default;
        virtual ~TestTimer() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestTimer, test_timer_fires)
{
    std::atomic<bool> fired {false};

    {
        Timer t([&fired] { fired = true; }, std::chrono::milliseconds(20));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }

    EXPECT_TRUE(fired.load());
}

TEST_F(TestTimer, test_timer_cancel)
{
    std::atomic<bool> fired {false};

    {
        Timer t([&fired] { fired = true; }, std::chrono::milliseconds(200));
        t.cancel();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(fired.load());
}

TEST_F(TestTimer, test_timer_destructor_cancels)
{
    std::atomic<bool> fired {false};

    {
        Timer t([&fired] { fired = true; }, std::chrono::milliseconds(100));
        // destroy before timeout → implicit cancel
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_FALSE(fired.load());
}

TEST_F(TestTimer, test_timer_throwing_callback)
{
    std::promise<void> fired;
    std::future<void> fired_future = fired.get_future();

    {
        Timer t(
            [&fired] {
                fired.set_value();
                throw std::runtime_error("timer callback failure");
            },
            std::chrono::milliseconds(20));
        EXPECT_EQ(fired_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    }
}

} // namespace test
