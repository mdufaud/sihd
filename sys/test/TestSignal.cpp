#include <atomic>

#include <gtest/gtest.h>

#include <sihd/sys/SigHandler.hpp>
#include <sihd/sys/SigWaiter.hpp>
#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timer.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

namespace
{
#if defined(__SIHD_WINDOWS__)
constexpr int sig_term = SIGTERM;
constexpr int sig_alt = SIGINT;
#else
constexpr int sig_term = SIGUSR1;
constexpr int sig_alt = SIGWINCH;
#endif

void send_self(int sig)
{
#if defined(__SIHD_WINDOWS__)
    // windows has no kill(); raise() delivers to the calling thread but the
    // handler sets process-global state that the pollers observe
    raise(sig);
#else
    kill(getpid(), sig);
#endif
}
} // namespace

class TestSignal: public ::testing::Test
{
    protected:
        TestSignal() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSignal() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            ::signal(sig_term, SIG_DFL);
            ::signal(sig_alt, SIG_DFL);
            signal::reset_all_received();
        }

        virtual void TearDown() {}
};

TEST_F(TestSignal, test_signal_handle)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    int sig = sig_term;
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
    sihd::util::Timestamp last_time = sig_status->time_received.load();

    if constexpr (build::is_emscripten)
    {
        // emscripten clock granularity is coarse: space the two raises so the second
        // time_received is strictly greater instead of tying
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    raise(sig);

    EXPECT_TRUE(signal::termination_received());

    sig_status = signal::status(sig);
    EXPECT_EQ(sig_status->received, 2U);
    EXPECT_GT(sig_status->time_received.load(), last_time);
    last_time = sig_status->time_received.load();

    EXPECT_TRUE(signal::should_stop());

    ASSERT_TRUE(signal::unhandle(sig));
}

TEST_F(TestSignal, test_signal_waiter)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    auto signal_waiter = [](int sig) {
        Timer t([sig] { send_self(sig); }, std::chrono::milliseconds(20));

        SigWaiter wait({.signal = sig, .timeout = std::chrono::milliseconds(100)});

        return wait.received_signal();
    };

    EXPECT_TRUE(signal_waiter(sig_term));
    EXPECT_TRUE(signal_waiter(sig_alt));
}

TEST_F(TestSignal, test_signal_watcher)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    std::atomic<int> sig = -1;

    SigWatcher watcher("sigint-watcher-test");

    watcher.add_signal(sig_term);
    watcher.set_polling_frequency(400);

    Synchronizer sync(2);
    Handler<SigWatcher *> handler([&sig, &sync](SigWatcher *watcher) {
        auto & catched_signals = watcher->catched_signals();
        if (catched_signals.empty())
            return;
        sig = catched_signals[0];
        SIHD_LOG(debug, "Watcher found signal {}", sig.load());
        (void)sync.sync(std::chrono::milliseconds(100));
    });
    watcher.add_observer(&handler);
    watcher.start();

    send_self(sig_term);

    ASSERT_TRUE(sync.sync(std::chrono::milliseconds(100)));

    watcher.stop();

    EXPECT_EQ(sig.load(), sig_term);
}

#if !defined(__SIHD_WINDOWS__)
// unix-only: this test relies on the signal's default action being "ignore"
// (SIGWINCH) so that an unhandled raise() does not terminate the process - no
// windows signal (SIGINT/SIGTERM/SIGABRT) has an ignore-by-default action
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
#endif

} // namespace test
