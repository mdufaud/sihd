#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/ObserverWatcher.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

class SomeObservable: public Observable<SomeObservable>
{
    public:
        int get_val() { return val; }

        void notify() { this->notify_observers(this); }

        int val = 0;
};

// records each observer callback: its letter on before, uppercase on after
class RecordingWatcher: public IObserverWatcher<SomeObservable>
{
    public:
        RecordingWatcher(std::string *seq_ptr): seq(seq_ptr) {}

        void watch(IHandler<SomeObservable *> *obs, char letter) { letters.emplace(obs, letter); }

        void before_observer(IHandler<SomeObservable *> *obs, [[maybe_unused]] SomeObservable *sender) override
        {
            seq->push_back(letters[obs]);
        }

        void after_observer(IHandler<SomeObservable *> *obs, [[maybe_unused]] SomeObservable *sender) override
        {
            seq->push_back(std::toupper(letters[obs]));
        }

        std::string *seq;
        std::map<IHandler<SomeObservable *> *, char> letters;
};

class CountingWatcher: public IObserverWatcher<SomeObservable>
{
    public:
        void before_observer(IHandler<SomeObservable *> *, [[maybe_unused]] SomeObservable *sender) override
        {
            ++calls;
        }

        void after_observer(IHandler<SomeObservable *> *, [[maybe_unused]] SomeObservable *sender) override { ++calls; }

        int calls = 0;
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

TEST_F(TestObservable, test_obs_watcher)
{
    std::string seq;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *obs) {});
    Handler<SomeObservable *> handler_b([]([[maybe_unused]] SomeObservable *obs) {});
    Handler<SomeObservable *> handler_c([]([[maybe_unused]] SomeObservable *obs) {});
    Handler<SomeObservable *> handler_d([]([[maybe_unused]] SomeObservable *obs) {});

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&handler_b);
    observable.add_observer(&handler_c);

    RecordingWatcher watcher(&seq);
    watcher.watch(&handler_a, 'a');
    watcher.watch(&handler_b, 'b');
    watcher.watch(&handler_c, 'c');
    watcher.watch(&handler_d, 'd');
    observable.set_watcher(&watcher);

    observable.notify();
    EXPECT_EQ(seq, "aAbBcC");

    observable.add_observer(&handler_d);
    seq.clear();
    observable.notify();
    EXPECT_EQ(seq, "aAbBcCdD");

    observable.set_watcher(nullptr);
    seq.clear();
    observable.notify();
    EXPECT_EQ(seq, "");
}

TEST_F(TestObservable, test_obs_watcher_self_removal)
{
    std::string seq;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *obs) {});
    Handler<SomeObservable *> handler_b([&](SomeObservable *obs) { obs->remove_observer(&handler_b); });
    Handler<SomeObservable *> handler_c([]([[maybe_unused]] SomeObservable *obs) {});

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&handler_b);
    observable.add_observer(&handler_c);

    RecordingWatcher watcher(&seq);
    watcher.watch(&handler_a, 'a');
    watcher.watch(&handler_b, 'b');
    watcher.watch(&handler_c, 'c');
    observable.set_watcher(&watcher);

    // the observer removing itself mid-notification is still watched until it returns
    observable.notify();
    EXPECT_EQ(seq, "aAbBcC");
    EXPECT_FALSE(observable.is_observer(&handler_b));

    seq.clear();
    observable.notify();
    EXPECT_EQ(seq, "aAcC");
}

TEST_F(TestObservable, test_obs_watcher_replaced_mid_notification)
{
    SomeObservable observable;
    CountingWatcher watcher1;
    CountingWatcher watcher2;
    Handler<SomeObservable *> switcher([&](SomeObservable *obs) { obs->set_watcher(&watcher2); });

    Handler<SomeObservable *> handler_a([&]([[maybe_unused]] SomeObservable *obs) {});
    observable.add_observer(&handler_a);
    observable.add_observer(&switcher);

    observable.set_watcher(&watcher1);

    // the watcher is read at every hook call: the replacement applies to the
    // remaining observers of the same notification
    observable.notify();
    EXPECT_EQ(watcher1.calls, 3);
    EXPECT_EQ(watcher2.calls, 1);

    observable.notify();
    EXPECT_EQ(watcher1.calls, 3);
    EXPECT_EQ(watcher2.calls, 5);
}

TEST_F(TestObservable, test_obs_watcher_replaced_and_destroyed)
{
    std::string seq;
    CountingWatcher watcher2;
    int watcher1_calls = 0;
    CountingWatcher *watcher1 = new CountingWatcher();
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_b([]([[maybe_unused]] SomeObservable *) {});
    SomeObservable observable;
    bool switched = false;
    Handler<SomeObservable *> switcher([&]([[maybe_unused]] SomeObservable *) {
        if (switched)
            return;
        switched = true;
        observable.set_watcher(&watcher2);
        watcher1_calls = watcher1->calls;
        // the replaced watcher is destroyed right away: it must not be
        // called again for the rest of the notification
        delete watcher1;
    });

    observable.add_observer(&handler_a);
    observable.add_observer(&switcher);
    observable.add_observer(&handler_b);
    observable.set_watcher(watcher1);

    observable.notify();
    EXPECT_EQ(watcher1_calls, 3);
    EXPECT_EQ(watcher2.calls, 3);

    observable.notify();
    EXPECT_EQ(watcher2.calls, 9);
}

TEST_F(TestObservable, test_obs_watcher_reattach_mid_notification)
{
    std::string seq;
    RecordingWatcher watcher(&seq);
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_c([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_b([&]([[maybe_unused]] SomeObservable *obs) {
        obs->set_watcher(nullptr);
        obs->set_watcher(&watcher);
    });
    watcher.watch(&handler_a, 'a');
    watcher.watch(&handler_b, 'b');
    watcher.watch(&handler_c, 'c');

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&handler_b);
    observable.add_observer(&handler_c);
    observable.set_watcher(&watcher);

    observable.notify();
    EXPECT_EQ(seq, "aAbBcC");
}

TEST_F(TestObservable, test_obs_watcher_recursive_notification)
{
    std::string seq;
    bool recursed = false;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_b([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> recursor([&](SomeObservable *obs) {
        if (recursed == false)
        {
            recursed = true;
            obs->notify();
        }
    });

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&recursor);
    observable.add_observer(&handler_b);

    RecordingWatcher watcher(&seq);
    watcher.watch(&handler_a, 'a');
    watcher.watch(&recursor, 'r');
    watcher.watch(&handler_b, 'b');
    observable.set_watcher(&watcher);

    observable.notify();
    EXPECT_EQ(seq, "aAraArRbBRbB");
}

TEST_F(TestObservable, test_obs_watcher_recursive_detach)
{
    std::string seq;
    bool recursed = false;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_b([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> recursor([&](SomeObservable *obs) {
        if (recursed == false)
        {
            recursed = true;
            obs->notify();
        }
    });
    Handler<SomeObservable *> detacher([&](SomeObservable *obs) { obs->set_watcher(nullptr); });

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&recursor);
    observable.add_observer(&detacher);
    observable.add_observer(&handler_b);

    RecordingWatcher watcher(&seq);
    watcher.watch(&handler_a, 'a');
    watcher.watch(&recursor, 'r');
    watcher.watch(&detacher, 'd');
    watcher.watch(&handler_b, 'b');
    observable.set_watcher(&watcher);

    // the inner notification detaches the watcher: the outer notification
    // ends without any watcher call
    observable.notify();
    EXPECT_EQ(seq, "aAraArRd");
}

TEST_F(TestObservable, test_obs_watcher_removed_and_detached)
{
    std::string seq;
    RecordingWatcher watcher(&seq);
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_c([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_b([&](SomeObservable *obs) {
        obs->remove_observer(&handler_c);
        obs->set_watcher(nullptr);
    });
    watcher.watch(&handler_a, 'a');
    watcher.watch(&handler_b, 'b');
    watcher.watch(&handler_c, 'c');

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&handler_b);
    observable.add_observer(&handler_c);
    observable.set_watcher(&watcher);

    observable.notify();
    EXPECT_EQ(seq, "aAb");
    EXPECT_FALSE(observable.is_observer(&handler_c));

    seq.clear();
    observable.notify();
    EXPECT_EQ(seq, "");
}

TEST_F(TestObservable, test_obs_watcher_detached_from_envelope)
{
    std::string seq;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    SomeObservable observable;
    observable.add_observer(&handler_a);

    ObserverWatcher<SomeObservable> watcher(
        [&](SomeObservable *obs) {
            seq += "n";
            obs->set_watcher(nullptr);
        },
        nullptr,
        nullptr,
        nullptr);
    observable.set_watcher(&watcher);

    // detached from the before hook: no observer is watched, no after hook
    observable.notify();
    EXPECT_EQ(seq, "n");
}

TEST_F(TestObservable, test_obs_watcher_switches_itself)
{
    std::string seq;
    CountingWatcher watcher2;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *) {});
    Handler<SomeObservable *> handler_b([]([[maybe_unused]] SomeObservable *) {});
    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&handler_b);

    bool switched = false;
    ObserverWatcher<SomeObservable> watcher(
        nullptr,
        nullptr,
        [&](IHandler<SomeObservable *> *, [[maybe_unused]] SomeObservable *) {
            seq += "<";
            if (switched == false)
            {
                switched = true;
                observable.set_watcher(&watcher2);
            }
        },
        [&](IHandler<SomeObservable *> *, [[maybe_unused]] SomeObservable *) { seq += ">"; });
    observable.set_watcher(&watcher);

    observable.notify();
    EXPECT_EQ(seq, "<");
    EXPECT_EQ(watcher2.calls, 3);

    observable.notify();
    EXPECT_EQ(seq, "<");
    EXPECT_EQ(watcher2.calls, 7);
}

TEST_F(TestObservable, test_obs_watcher_lambdas)
{
    std::string seq;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *obs) {});
    Handler<SomeObservable *> handler_b([]([[maybe_unused]] SomeObservable *obs) {});

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&handler_b);

    ObserverWatcher<SomeObservable> watcher(
        [&](SomeObservable *) { seq += "n"; },
        nullptr,
        [&](IHandler<SomeObservable *> *obs, SomeObservable *) { seq += obs == &handler_a ? "<a" : "<b"; },
        [&](IHandler<SomeObservable *> *, SomeObservable *) { seq += ">"; });

    observable.set_watcher(&watcher);
    observable.notify();
    EXPECT_EQ(seq, "n<a><b>");

    seq.clear();
    ObserverWatcher<SomeObservable> empty;
    observable.set_watcher(&empty);
    observable.notify();
    EXPECT_EQ(seq, "");

    observable.set_watcher(nullptr);
    observable.notify();
    EXPECT_EQ(seq, "");
}

TEST_F(TestObservable, test_obs_watcher_detached_mid_notification)
{
    std::string seq;
    Handler<SomeObservable *> handler_a([]([[maybe_unused]] SomeObservable *obs) {});
    Handler<SomeObservable *> detacher([&](SomeObservable *obs) {
        obs->set_watcher(nullptr);
        seq += "x";
    });

    SomeObservable observable;
    observable.add_observer(&handler_a);
    observable.add_observer(&detacher);

    RecordingWatcher watcher(&seq);
    watcher.watch(&handler_a, 'a');
    watcher.watch(&detacher, 'd');
    observable.set_watcher(&watcher);

    // the observer detaching the watcher mid-notification is not called again
    observable.notify();
    EXPECT_EQ(seq, "aAdx");

    seq.clear();
    observable.notify();
    EXPECT_EQ(seq, "x");
}

} // namespace test
