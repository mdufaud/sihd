#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/util/StepWorker.hpp>

namespace test
{
using namespace sihd::util;

class TestStepWorker: public ::testing::Test
{
    protected:
        TestStepWorker() = default;
        virtual ~TestStepWorker() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestStepWorker, test_stepworker_frequency)
{
    std::atomic<int> count {0};

    StepWorker worker([&count]() -> bool {
        count++;
        return true;
    });

    EXPECT_TRUE(worker.set_frequency(100));
    EXPECT_TRUE(worker.start_sync_worker("test-step"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    worker.stop_worker();

    EXPECT_GT(count.load(), 0);
}

TEST_F(TestStepWorker, test_stepworker_pause_resume)
{
    std::atomic<int> count {0};

    StepWorker worker([&count]() -> bool {
        count++;
        return true;
    });

    EXPECT_TRUE(worker.set_frequency(200));
    EXPECT_TRUE(worker.start_sync_worker("test-step-pause"));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int before_pause = count.load();
    EXPECT_GT(before_pause, 0);

    worker.pause_worker();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int during_pause = count.load();

    worker.resume_worker();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int after_resume = count.load();

    worker.stop_worker();

    EXPECT_GE(during_pause - before_pause, 0);
    EXPECT_GT(after_resume, during_pause);
}

TEST_F(TestStepWorker, test_stepworker_callbacks)
{
    bool setup_called = false;
    bool teardown_called = false;

    StepWorker worker([]() -> bool { return true; });

    worker.set_frequency(100);
    worker.set_callback_setup([&setup_called] { setup_called = true; });
    worker.set_callback_teardown([&teardown_called] { teardown_called = true; });

    EXPECT_TRUE(worker.start_sync_worker("test-step-cb"));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    worker.stop_worker();

    EXPECT_TRUE(setup_called);
    EXPECT_TRUE(teardown_called);
}

} // namespace test
