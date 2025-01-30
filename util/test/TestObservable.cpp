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
        TestObservable() { sihd::util::LoggerManager::stream(); }

        virtual ~TestObservable() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void handle(SomeObservable *obs)
        {
            this->val = obs->get_val();
            obs->remove_observer(this);
            ++called;
        }

        int val = 0;
        int called = 0;
};

TEST_F(TestObservable, test_obs_multiple)
{
    Handler<SomeObservable *> handler_add_one([&](SomeObservable *obs) -> void {
        obs->val++;
        SIHD_TRACE("adding 1");
    });
    Handler<SomeObservable *> handler_add_two([&](SomeObservable *obs) -> void {
        SIHD_TRACE("adding 2");
        obs->val += 2;
    });
    Handler<SomeObservable *> handler_add_three([&](SomeObservable *obs) -> void {
        SIHD_TRACE("adding 3");
        obs->val += 3;
    });

    SomeObservable observable;
    observable.add_observer(&handler_add_one);
    observable.add_observer(&handler_add_two);
    observable.add_observer(&handler_add_three);

    EXPECT_EQ(observable.val, 0);
    observable.notify();
    EXPECT_EQ(observable.val, 6);

    Handler<SomeObservable *> handler_remover_one([&](SomeObservable *obs) -> void {
        SIHD_TRACE("removing 1");
        obs->remove_observer(&handler_add_one);
        obs->val = -10;
    });
    Handler<SomeObservable *> handler_remover_two([&](SomeObservable *obs) -> void {
        SIHD_TRACE("removing 2");
        obs->remove_observer(&handler_add_two);
        if (obs->val == -10)
            obs->val = 1337;
        SIHD_TRACE("removing 3");
        obs->remove_observer(&handler_add_three);
    });

    constexpr bool add_to_front = true;
    observable.add_observer(&handler_remover_two, add_to_front);
    observable.add_observer(&handler_remover_one, add_to_front);

    observable.val = 0;
    observable.notify();
    EXPECT_EQ(observable.val, 1337);

    observable.remove_observer(&handler_remover_two);
    observable.remove_observer(&handler_remover_one);

    Handler<SomeObservable *> handler_adder([&](SomeObservable *obs) -> void {
        SIHD_TRACE("adding all back");
        obs->add_observer(&handler_add_one);
        obs->add_observer(&handler_add_two);
        obs->add_observer(&handler_add_three);
        SIHD_TRACE("removing self");
        obs->remove_observer(&handler_adder);
    });
    observable.add_observer(&handler_adder);

    observable.val = 0;
    observable.notify();
    EXPECT_EQ(observable.val, 6);
}

TEST_F(TestObservable, test_obs_inheritance)
{
    SomeObservable observable;
    observable.val = 1337;
    observable.add_observer(this);
    // should be ignored
    observable.add_observer(this);
    observable.add_observer(this);
    observable.add_observer(this);
    EXPECT_EQ(this->val, 0);
    EXPECT_EQ(this->called, 0);
    observable.notify();
    EXPECT_EQ(this->val, 1337);
    EXPECT_EQ(this->val, observable.val);
    EXPECT_EQ(this->called, 1);

    observable.val = 424242;
    observable.notify();
    EXPECT_EQ(this->val, 1337);
    EXPECT_EQ(this->called, 1);
}

TEST_F(TestObservable, test_obs_lambda)
{
    SomeObservable observable;
    observable.val = 1337;
    int val = 0;
    Handler<SomeObservable *> handler([&](SomeObservable *obs) -> void { val = obs->get_val(); });
    observable.add_observer(&handler);
    EXPECT_EQ(val, 0);
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
