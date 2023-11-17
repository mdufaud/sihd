#include <utility>

#include <gtest/gtest.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Decorator.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

class SomeObservable: public Observable<SomeObservable>
{
    public:
        SomeObservable() {};
        ~SomeObservable() {};

        int get_val() { return val; }

        void notify() { this->notify_observers(this); }

        int val = 0;
};

class TestObservable: public ::testing::Test,
                      public IHandler<SomeObservable *>
{
    protected:
        TestObservable() { sihd::util::LoggerManager::basic(); }

        virtual ~TestObservable() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void handle(SomeObservable *obs)
        {
            this->val = obs->get_val();
            obs->remove_observer_inside_notification(this);
        }

        int val = 0;
};

TEST_F(TestObservable, test_obs_inheritance)
{
    SomeObservable observable;
    observable.val = 1337;
    observable.add_observer(this);
    observable.add_observer(this);
    observable.add_observer(this);
    observable.add_observer(this);
    EXPECT_EQ(this->val, 0);
    observable.notify();
    EXPECT_EQ(this->val, observable.val);

    observable.val = 424242;
    observable.notify();
    EXPECT_EQ(this->val, 1337);
}

TEST_F(TestObservable, test_obs_lambda)
{
    SomeObservable observable;
    observable.val = 1337;
    int val = 0;
    Handler<SomeObservable *> handler([&](SomeObservable *obs) -> void { val = obs->get_val(); });
    EXPECT_EQ(val, 0);
    observable.add_observer(&handler);
    observable.notify();
    EXPECT_EQ(val, 1337);
    observable.val = 4242;
    observable.remove_observer(&handler);
    observable.notify();
    EXPECT_EQ(val, 1337);
}

TEST_F(TestObservable, test_obs_decorator)
{
    SteadyClock clock;
    SomeObservable observable;
    Timestamp ts_begin(0);
    Timestamp ts_handler(0);
    Timestamp ts_end(0);

    Decorator<SomeObservable> decorator;

    Handler<SomeObservable *> handler([&]([[maybe_unused]] SomeObservable *obs) { ts_handler = clock.now(); });
    observable.add_observer(&handler);

    decorator.decorate(&observable);

    decorator.set_handler_begin([&ts_begin, &clock](auto) { ts_begin = clock.now(); });
    decorator.set_handler_end([&ts_end, &clock](auto) { ts_end = clock.now(); });

    EXPECT_EQ(ts_handler, ts_begin);
    EXPECT_EQ(ts_handler, ts_end);

    observable.notify();

    EXPECT_LT(ts_begin, ts_handler);
    EXPECT_LT(ts_handler, ts_end);

    decorator.reset();

    observable.notify();

    EXPECT_LT(ts_begin, ts_handler);
    EXPECT_LT(ts_end, ts_handler);
}

} // namespace test
