#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/ObserverCallback.hpp>
#include <utility>

namespace test
{
    LOGGER;
    using namespace sihd::util;

    class SomeObservable:    public Observable<SomeObservable>
    {
        public:
            SomeObservable() {};
            ~SomeObservable() {};

            int    get_val()
            {
                return val;
            }

            void    notify() { this->notify_observers(this); }

            int val = 0;
    };

    class TestObservable:   public ::testing::Test, virtual public IObserver<SomeObservable>
    {
        protected:
            TestObservable()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestObservable()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            void observable_changed(SomeObservable *obs)
            {
                this->val = obs->get_val();
                obs->remove_observer(this);
            }

            int    val = 0;
    };

    TEST_F(TestObservable, test_obs_inheritance)
    {
        SomeObservable obj;
        obj.val = 1337;
        obj.add_observer(this);
        EXPECT_EQ(this->val, 0);
        obj.notify();
        EXPECT_EQ(this->val, obj.val);

        obj.val = 424242;
        obj.notify();
        EXPECT_EQ(this->val, 1337);
    }

    TEST_F(TestObservable, test_obs_lambda)
    {
        SomeObservable obj;
        obj.val = 1337;
        int val = 0;
        ObserverCallback<SomeObservable> cb([&] (SomeObservable *obs) -> void
        {
            val = obs->get_val();
        });
        EXPECT_EQ(val, 0);
        obj.add_observer(&cb);
        obj.notify();
        EXPECT_EQ(val, 1337);
        obj.val = 4242;
        obj.remove_observer(&cb);
        obj.notify();
        EXPECT_EQ(val, 1337);
    }
}
