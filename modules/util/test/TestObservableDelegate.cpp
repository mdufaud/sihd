#include <gtest/gtest.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/ObservableDelegate.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");
using namespace sihd::util;

struct DelegateSource
{
        explicit DelegateSource(int v): val(v) {}
        int val;
};

class TestObservableDelegate: public ::testing::Test
{
    protected:
        TestObservableDelegate() { LoggerManager::stream(); }
        virtual ~TestObservableDelegate() { LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestObservableDelegate, test_observable_delegate_notify)
{
    DelegateSource source {42};
    ObservableDelegate<DelegateSource> delegate(&source);

    int received_val = -1;
    Handler<DelegateSource *> handler([&](DelegateSource *ptr) { received_val = ptr->val; });

    delegate.add_observer(&handler);
    delegate.notify_observers_delegate();

    EXPECT_EQ(received_val, 42);

    source.val = 99;
    delegate.notify_observers_delegate();
    EXPECT_EQ(received_val, 99);
}

TEST_F(TestObservableDelegate, test_observable_delegate_base_view)
{
    DelegateSource source {0};
    ObservableDelegate<DelegateSource> delegate(&source);

    Observable<DelegateSource> & observable = delegate;

    int call_count = 0;
    Handler<DelegateSource *> handler([&]([[maybe_unused]] DelegateSource *ptr) { ++call_count; });

    EXPECT_FALSE(observable.is_observer(&handler));
    EXPECT_TRUE(observable.add_observer(&handler));
    EXPECT_TRUE(observable.is_observer(&handler));

    delegate.notify_observers_delegate();
    EXPECT_EQ(call_count, 1);

    observable.remove_observer(&handler);
    EXPECT_FALSE(observable.is_observer(&handler));

    delegate.notify_observers_delegate();
    EXPECT_EQ(call_count, 1);
}

} // namespace test
