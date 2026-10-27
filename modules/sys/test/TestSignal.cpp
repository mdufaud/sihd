#include <array>
#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/SigHandler.hpp>
#include <sihd/sys/SigThreadBlocker.hpp>
#include <sihd/sys/SigWaiter.hpp>
#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Timer.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <sys/wait.h>
# include <unistd.h>
#endif

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
    // no kill() on windows: raise() hits the calling thread, the handler sets process-global state
    raise(sig);
#else
    kill(getpid(), sig);
#endif
}
} // namespace

class TestSignal: public ::testing::Test
{
    protected:
        TestSignal() { LoggerManager::stream(); }

        virtual ~TestSignal() { LoggerManager::clear_loggers(); }

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
    // a forked child inherits this buffer: leftovers must not pollute its captured stdout
    fflush(stdout);

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
    Timestamp last_time = sig_status->time_received.load();

    if constexpr (build::is_emscripten)
    {
        // emscripten clock granularity is coarse: space the raises so time_received is strictly greater
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
        Timer timer([sig] { send_self(sig); }, std::chrono::milliseconds(20));

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

TEST_F(TestSignal, test_signal_watcher_stop_skips_the_polling_interval)
{
    if constexpr (!build::is_linux || build::is_android || build::is_emscripten)
        GTEST_SKIP() << "only the signalfd watcher waits without an interval";

    SigWatcher watcher("stop-watcher-test");
    ASSERT_TRUE(watcher.add_signal(sig_term));
    ASSERT_TRUE(watcher.set_polling_frequency(0.1));
    ASSERT_TRUE(watcher.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    SteadyClock clock;
    const Timestamp before = clock.now();
    EXPECT_TRUE(watcher.stop());

    EXPECT_LT(clock.now() - before, Duration(time::sec(1)));
}

#if !defined(__SIHD_WINDOWS__)
// unix-only: SIGWINCH is ignored by default so an unhandled raise() does not terminate; no windows signal is
TEST_F(TestSignal, test_signal_tmp)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    int sig = SIGWINCH;

    auto sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 0u);

    SigHandler handler;

    (void)handler.handle(sig);

    raise(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1u);

    (void)handler.unhandle();

    raise(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1u);

    (void)signal::handle(sig);

    {
        SigHandler tmp_sig_handling(sig);

        raise(sig);

        sig_status = signal::status(sig);
        ASSERT_TRUE(sig_status);
        EXPECT_EQ(sig_status->received, 2u);
    }

    raise(sig);

    (void)signal::unhandle(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 3u);
}

TEST_F(TestSignal, test_block_thread_set)
{
    const std::array sigs = {sig_term, sig_alt};
    ASSERT_TRUE(signal::block_thread(sigs));

    raise(sig_term);
    raise(sig_alt);

    sigset_t pending;
    sigemptyset(&pending);
    ASSERT_EQ(sigpending(&pending), 0);
    EXPECT_EQ(sigismember(&pending, sig_term), 1);
    EXPECT_EQ(sigismember(&pending, sig_alt), 1);

    // unblock drains pending so the signals are never delivered
    ASSERT_TRUE(signal::unblock_thread(sigs));

    sigemptyset(&pending);
    ASSERT_EQ(sigpending(&pending), 0);
    EXPECT_EQ(sigismember(&pending, sig_term), 0);
    EXPECT_EQ(sigismember(&pending, sig_alt), 0);
}

TEST_F(TestSignal, test_sig_thread_blocker_restores_mask)
{
    // pre-block sig_term so we can verify the prior mask is restored, not cleared
    sigset_t pre;
    sigemptyset(&pre);
    sigaddset(&pre, sig_term);
    ASSERT_EQ(pthread_sigmask(SIG_BLOCK, &pre, nullptr), 0);

    {
        SigThreadBlocker blocker({sig_term, sig_alt});
        EXPECT_TRUE(blocker.is_blocking());

        sigset_t cur;
        sigemptyset(&cur);
        ASSERT_EQ(pthread_sigmask(SIG_BLOCK, nullptr, &cur), 0);
        EXPECT_EQ(sigismember(&cur, sig_term), 1);
        EXPECT_EQ(sigismember(&cur, sig_alt), 1);
    }

    sigset_t after;
    sigemptyset(&after);
    ASSERT_EQ(pthread_sigmask(SIG_BLOCK, nullptr, &after), 0);
    EXPECT_EQ(sigismember(&after, sig_term), 1);
    EXPECT_EQ(sigismember(&after, sig_alt), 0);

    pthread_sigmask(SIG_UNBLOCK, &pre, nullptr);
}
#endif

TEST_F(TestSignal, test_signal_categories_and_names)
{
    EXPECT_TRUE(signal::is_category_termination(SIGTERM));
    EXPECT_TRUE(signal::is_category_termination(SIGINT));
    EXPECT_TRUE(signal::is_category_dump(SIGABRT));
    EXPECT_TRUE(signal::is_category_dump(SIGILL));
    EXPECT_TRUE(signal::is_category_dump(SIGSEGV));
    EXPECT_FALSE(signal::is_category_stop(SIGTERM));
    EXPECT_FALSE(signal::is_category_dump(SIGTERM));
    EXPECT_FALSE(signal::is_category_termination(SIGABRT));
    EXPECT_FALSE(signal::is_category_stop(0));
    EXPECT_FALSE(signal::is_category_termination(0));
    EXPECT_FALSE(signal::is_category_dump(0));

#if !defined(__SIHD_WINDOWS__)
    EXPECT_TRUE(signal::is_category_stop(SIGSTOP));
    EXPECT_TRUE(signal::is_category_stop(SIGTSTP));
    EXPECT_TRUE(signal::is_category_stop(SIGTTIN));
    EXPECT_TRUE(signal::is_category_termination(SIGUSR1));
    EXPECT_TRUE(signal::is_category_termination(SIGHUP));
    EXPECT_TRUE(signal::is_category_termination(SIGPIPE));
    EXPECT_TRUE(signal::is_category_dump(SIGQUIT));
    EXPECT_TRUE(signal::is_category_dump(SIGFPE));
    EXPECT_FALSE(signal::is_category_termination(SIGSTOP));
    EXPECT_FALSE(signal::is_category_dump(SIGTERM));
#endif

    EXPECT_FALSE(signal::name(SIGTERM).empty());
    EXPECT_FALSE(signal::name(SIGINT).empty());
    EXPECT_FALSE(signal::name(SIGABRT).empty());
#if !defined(__SIHD_WINDOWS__)
    EXPECT_FALSE(signal::name(SIGUSR1).empty());
#endif
}

TEST_F(TestSignal, test_signal_reset_received_single)
{
    ASSERT_TRUE(signal::handle(sig_term));
    ASSERT_TRUE(signal::handle(sig_alt));
    Defer unhandle_all([] {
        (void)signal::unhandle(sig_term);
        (void)signal::unhandle(sig_alt);
        signal::reset_all_received();
    });

    raise(sig_term);
    raise(sig_alt);
    auto first = signal::status(sig_term);
    ASSERT_TRUE(first);
    EXPECT_GE(first->received.load(), 1U);
    auto second = signal::status(sig_alt);
    ASSERT_TRUE(second);
    EXPECT_GE(second->received.load(), 1U);

    signal::reset_received(sig_term);

    auto reset = signal::status(sig_term);
    ASSERT_TRUE(reset);
    EXPECT_EQ(reset->received.load(), 0U);
    auto kept = signal::status(sig_alt);
    ASSERT_TRUE(kept);
    EXPECT_GE(kept->received.load(), 1U);
}

TEST_F(TestSignal, test_signal_waiter_timeout)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    SigWaiter wait({.timeout = std::chrono::milliseconds(50)});
    EXPECT_FALSE(wait.received_signal());
}

TEST_F(TestSignal, test_signal_watcher_rm_signal)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    std::atomic<int> sig = -1;
    Synchronizer sync(2);
    Handler<SigWatcher *> handler([&sig, &sync](SigWatcher *watcher) {
        auto & catched_signals = watcher->catched_signals();
        if (catched_signals.empty())
            return;
        sig = catched_signals[0];
        (void)sync.sync(std::chrono::milliseconds(100));
    });

    SigWatcher watcher("sig-watcher-rm-test");
    watcher.set_polling_frequency(400);
    watcher.add_observer(&handler);
    ASSERT_TRUE(watcher.add_signal(sig_term));
    watcher.start();
    Defer stop_watcher([&] {
        watcher.stop();
#if !defined(__SIHD_WINDOWS__)
        (void)signal::unblock_thread(sig_term);
#endif
    });

    send_self(sig_term);
    ASSERT_TRUE(sync.sync(std::chrono::milliseconds(200)));
    ASSERT_EQ(sig.load(), sig_term);

    ASSERT_TRUE(watcher.rm_signal(sig_term));
#if !defined(__SIHD_WINDOWS__)
    sig = -1;
    send_self(sig_term);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(sig.load(), -1);
#endif
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestSignal, test_signal_exit_config_exits_with_sig_number)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "fork under valgrind";

    signal::set_exit_config({.on_termination = true, .exit_with_sig_number = true});
    Defer reset_config([] { signal::set_exit_config(signal::SigExitConfig {}); });

    fflush(stdout);
    const pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0)
    {
        signal::reset_all_received();
        (void)signal::handle(SIGTERM);
        kill(getpid(), SIGTERM);
        _exit(100);
    }
    int wstatus = 0;
    ASSERT_EQ(waitpid(pid, &wstatus, 0), pid);
    EXPECT_TRUE(WIFEXITED(wstatus));
    EXPECT_EQ(WEXITSTATUS(wstatus), SIGTERM);
}
#endif

TEST_F(TestSignal, test_block_thread_not_supported_on_windows)
{
    if constexpr (!build::is_windows)
        GTEST_SKIP() << "per-thread signal mask exists outside windows";

    const std::array sigs = {sig_term, sig_alt};

    auto blocked = signal::block_thread(sig_term);
    ASSERT_FALSE(blocked);
    EXPECT_EQ(blocked.error().code, ErrorCode::not_supported);

    auto blocked_set = signal::block_thread(sigs);
    ASSERT_FALSE(blocked_set);
    EXPECT_EQ(blocked_set.error().code, ErrorCode::not_supported);

    auto unblocked = signal::unblock_thread(sig_term);
    ASSERT_FALSE(unblocked);
    EXPECT_EQ(unblocked.error().code, ErrorCode::not_supported);

    auto unblocked_set = signal::unblock_thread(sigs);
    ASSERT_FALSE(unblocked_set);
    EXPECT_EQ(unblocked_set.error().code, ErrorCode::not_supported);
}

} // namespace test
