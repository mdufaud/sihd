#include <gtest/gtest.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/Timer.hpp>
#include <sihd/util/signal.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
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
    auto test_lambda = [](int sig) {
        Timer t([sig] { raise(sig); }, std::chrono::milliseconds(5));

        SigWaiter wait(
            {.sig = sig, .polling_frequency = std::chrono::milliseconds(1), .timeout = std::chrono::milliseconds(100)});

        EXPECT_TRUE(wait.received_signal());
    };

    test_lambda(1);
    test_lambda(63);
}

TEST_F(TestSignal, test_signal_watcher)
{
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

    watcher.set_start_synchronised(true);
    watcher.start();

    raise(SIGUSR1);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(sig, SIGUSR1);
}

TEST_F(TestSignal, test_signal_tmp)
{
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
