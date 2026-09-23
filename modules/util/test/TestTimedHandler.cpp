#include <gtest/gtest.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/TimedHandler.hpp>

#include "clock/clock_helper.hpp"

namespace test
{
using namespace sihd::util;

class SomeObservable: public Observable<SomeObservable>
{
    public:
        int val = 0;

        void notify() { this->notify_observers(this); }
};

class SomeHandler: public IHandler<SomeObservable *>
{
    public:
        void handle(SomeObservable *obs) override
        {
            ++called;
            obs->val += 1;
        }

        int called = 0;
};

TEST(TestTimedHandler, test_timed_handler)
{
    SomeObservable observable;
    SomeHandler handler;
    // a fake clock: a measured duration cannot be zero whatever the platform's
    // timer resolution
    FakeClock clock;
    TimedHandler<SomeObservable> timed(&handler, "handler", &clock);
    observable.add_observer(&timed);

    observable.notify();
    EXPECT_EQ(handler.called, 1);
    EXPECT_EQ(observable.val, 1);
    EXPECT_EQ(timed.stat().samples, 1u);
    EXPECT_EQ(timed.wrapped(), &handler);
    EXPECT_EQ(timed.label(), "handler");
    EXPECT_GT(timed.last(), 0);

    observable.notify();
    EXPECT_EQ(handler.called, 2);
    EXPECT_EQ(observable.val, 2);
    EXPECT_EQ(timed.stat().samples, 2u);

    observable.remove_observer(&timed);
    observable.notify();
    EXPECT_EQ(handler.called, 2);
    EXPECT_EQ(observable.val, 2);
}

TEST(TestTimedHandler, test_inactive)
{
    SomeObservable observable;
    SomeHandler handler;
    FakeClock clock;
    TimedHandler<SomeObservable> timed(&handler, "handler", &clock);
    observable.add_observer(&timed);

    observable.notify();
    EXPECT_EQ(timed.stat().samples, 1u);
    const time::UnixTime last_sampled = timed.last();
    EXPECT_GT(last_sampled, 0);

    timed.set_active(false);
    EXPECT_FALSE(timed.active());
    observable.notify();
    EXPECT_EQ(handler.called, 2);
    EXPECT_EQ(observable.val, 2);
    EXPECT_EQ(timed.stat().samples, 1u);
    EXPECT_EQ(timed.last(), last_sampled);

    timed.set_active(true);
    EXPECT_TRUE(timed.active());
    observable.notify();
    EXPECT_EQ(handler.called, 3);
    EXPECT_EQ(timed.stat().samples, 2u);
}

} // namespace test
