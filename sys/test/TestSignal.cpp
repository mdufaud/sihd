#include <gtest/gtest.h>

#include <sihd/sys/SigHandler.hpp>
#include <sihd/sys/SigWaiter.hpp>
#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timer.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestSignal: public ::testing::Test
{
    protected:
        TestSignal() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSignal() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            ::signal(SIGUSR1, SIG_DFL);
            ::signal(SIGWINCH, SIG_DFL);
            signal::reset_all_received();
        }

        virtual void TearDown() {}
};

TEST_F(TestSignal, test_signal_handle)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    int sig = SIGUSR1;
    ASSERT_TRUE(signal::handle(sig));

    fmt::print("{}", signal::status_str());

    EXPECT_FALSE(signal::stop_received());
    EXPECT_FALSE(signal::termination_received());
    EXPECT_FALSE(signal::dump_received());

    raise(sig);

    EXPECT_FALSE(signal::stop_received());
    EXPECT_FALSE(signal::dump_received());
    EXPECT_TRUE(signal::termination_received());

    auto sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1U);
    time_t last_time = sig_status->time_received;

    raise(sig);

    EXPECT_TRUE(signal::termination_received());

    sig_status = signal::status(sig);
    EXPECT_EQ(sig_status->received, 2U);
    EXPECT_GT(sig_status->time_received, last_time);
    last_time = sig_status->time_received;

    EXPECT_TRUE(signal::should_stop());

    ASSERT_TRUE(signal::unhandle(sig));
}

TEST_F(TestSignal, test_signal_waiter)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    auto signal_waiter = [](int sig) {
        Timer t([sig] { kill(getpid(), sig); }, std::chrono::milliseconds(20));

        SigWaiter wait({.signal = sig, .timeout = std::chrono::milliseconds(100)});

        return wait.received_signal();
    };

    EXPECT_TRUE(signal_waiter(SIGHUP));
    EXPECT_TRUE(signal_waiter(SIGWINCH));
}

TEST_F(TestSignal, test_signal_watcher)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    int sig = -1;

    SigWatcher watcher("sigint-watcher-test");

    watcher.add_signal(SIGUSR1);
    watcher.set_polling_frequency(30);

    Handler<SigWatcher *> handler([&sig](SigWatcher *watcher) {
        auto & catched_signals = watcher->catched_signals();
        if (catched_signals.empty())
            return;
        sig = catched_signals[0];
        SIHD_LOG(debug, "Watcher found signal {}", sig);
    });
    watcher.add_observer(&handler);

    watcher.start();

    // Give worker time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    kill(getpid(), SIGUSR1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(sig, SIGUSR1);
}

TEST_F(TestSignal, test_signal_tmp)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    int sig = SIGWINCH;

    auto sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 0u);

    SigHandler handler;

    handler.handle(sig);

    raise(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1u);

    handler.unhandle();

    raise(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1u);

    signal::handle(sig);

    {
        SigHandler tmp_sig_handling(sig);

        raise(sig);

        sig_status = signal::status(sig);
        ASSERT_TRUE(sig_status);
        EXPECT_EQ(sig_status->received, 2u);
    }

    // still handling signal

    raise(sig);

    signal::unhandle(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 3u);
}

} // namespace test
