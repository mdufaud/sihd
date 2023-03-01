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
        TestSignal() { sihd::util::LoggerManager::basic(); }

        virtual ~TestSignal() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { signal::reset_all_received(); }

        virtual void TearDown() {}
};

TEST_F(TestSignal, test_signal_handle)
{
    int sig = SIGINT;
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
    const Timestamp polling_frequency = std::chrono::milliseconds(1);
    int sig = -1;

    SigWatcher watcher(
        {SIGINT, 10000},
        [&sig](int signal_found) {
            SIHD_LOG(debug, "Watcher found signal {}", signal_found);
            sig = signal_found;
        },
        polling_frequency);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    raise(SIGINT);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(sig, SIGINT);
}

TEST_F(TestSignal, test_signal_tmp)
{
    int sig = SIGWINCH;

    auto sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 0);

    SigHandler handler;

    handler.handle(sig);

    raise(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1);

    handler.unhandle();

    raise(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 1);

    signal::handle(sig);

    {
        SigHandler tmp_sig_handling(sig);

        raise(sig);

        sig_status = signal::status(sig);
        ASSERT_TRUE(sig_status);
        EXPECT_EQ(sig_status->received, 2);
    }

    // still handling signal

    raise(sig);

    signal::unhandle(sig);

    sig_status = signal::status(sig);
    ASSERT_TRUE(sig_status);
    EXPECT_EQ(sig_status->received, 3);
}

} // namespace test
