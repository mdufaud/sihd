#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/ObserverWaiter.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");
using namespace sihd::util;

struct SimpleSource: public Observable<SimpleSource>
{
        void notify() { this->notify_observers(this); }
};

class TestObserverWaiter: public ::testing::Test
{
    protected:
        TestObserverWaiter() { sihd::util::LoggerManager::stream(); }
        virtual ~TestObserverWaiter() { sihd::util::LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestObserverWaiter, test_observer_waiter_wait)
{
    SimpleSource src;
    ObserverWaiter<SimpleSource> waiter(&src);

    std::thread t([&src] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        src.notify();
    });

    waiter.wait(1);
    t.join();

    EXPECT_EQ(waiter.notifications(), 1u);
    EXPECT_EQ(waiter.observing(), &src);
}

TEST_F(TestObserverWaiter, test_observer_waiter_wait_for_timeout)
{
    SimpleSource src;
    ObserverWaiter<SimpleSource> waiter(&src);

    const bool triggered = waiter.wait_for(std::chrono::milliseconds(20), 1);

    EXPECT_FALSE(triggered);
    EXPECT_EQ(waiter.notifications(), 0u);
}

TEST_F(TestObserverWaiter, test_observer_waiter_wait_multiple)
{
    SimpleSource src;
    ObserverWaiter<SimpleSource> waiter(&src);

    constexpr int target = 5;
    std::thread t([&src] {
        for (int i = 0; i < target; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            src.notify();
        }
    });

    waiter.wait(target);
    t.join();

    EXPECT_EQ(waiter.notifications(), static_cast<uint32_t>(target));
}

TEST_F(TestObserverWaiter, test_observer_waiter_clear)
{
    SimpleSource src;
    ObserverWaiter<SimpleSource> waiter(&src);

    src.notify();
    waiter.wait_nb(1);

    waiter.clear();

    EXPECT_EQ(waiter.observing(), nullptr);
    EXPECT_EQ(waiter.notifications(), 0u);

    // Source should not crash when notified after clear
    src.notify();
    EXPECT_EQ(waiter.notifications(), 0u);
}

TEST_F(TestObserverWaiter, test_observer_waiter_observe_new)
{
    SimpleSource src1;
    SimpleSource src2;
    ObserverWaiter<SimpleSource> waiter(&src1);

    src1.notify();
    waiter.wait_nb(1);
    EXPECT_EQ(waiter.notifications(), 1u);

    waiter.observe(&src2);
    EXPECT_EQ(waiter.observing(), &src2);
    EXPECT_EQ(waiter.notifications(), 0u);

    std::thread t([&src2] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        src2.notify();
    });
    waiter.wait(1);
    t.join();

    EXPECT_EQ(waiter.notifications(), 1u);
    // src1 no longer notifies waiter
    src1.notify();
    EXPECT_EQ(waiter.notifications(), 1u);
}

} // namespace test
