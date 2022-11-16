#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Handler.hpp>
#include <utility>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;

    class SomeObservable: public Observable<SomeObservable>
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

    class TestObservable: public ::testing::Test, public IHandler<SomeObservable *>
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

            void handle(SomeObservable *obs)
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
        Handler<SomeObservable *> handler([&] (SomeObservable *obs) -> void
        {
            val = obs->get_val();
        });
        EXPECT_EQ(val, 0);
        obj.add_observer(&handler);
        obj.notify();
        EXPECT_EQ(val, 1337);
        obj.val = 4242;
        obj.remove_observer(&handler);
        obj.notify();
        EXPECT_EQ(val, 1337);
    }
}
