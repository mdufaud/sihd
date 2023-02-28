#include <gtest/gtest.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigWaiter.hpp>
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
    EXPECT_TRUE(signal::handle(sig));
    EXPECT_FALSE(signal::stop_received());
    EXPECT_FALSE(signal::termination_received());
    EXPECT_FALSE(signal::dump_received());

    raise(sig);

    EXPECT_FALSE(signal::stop_received());
    EXPECT_FALSE(signal::dump_received());
    EXPECT_TRUE(signal::termination_received());

    auto term_status = signal::status(sig);
    ASSERT_TRUE(term_status);
    EXPECT_EQ(term_status->received, 1U);
    time_t last_time = term_status->time_received;

    raise(sig);

    EXPECT_TRUE(signal::termination_received());

    term_status = signal::status(sig);
    EXPECT_EQ(term_status->received, 2U);
    EXPECT_GT(term_status->time_received, last_time);
    last_time = term_status->time_received;

    EXPECT_TRUE(signal::unhandle(sig));
}

TEST_F(TestSignal, test_signal_waiter)
{
    {
        int sig = 1;

        Timer t([sig] { raise(sig); }, std::chrono::milliseconds(5));

        SigWaiter wait(
            {.sig = sig, .polling_frequency = std::chrono::milliseconds(1), .timeout = std::chrono::milliseconds(100)});

        EXPECT_TRUE(wait.received_signal());
    }

    {
        int sig = 63;

        Timer t([sig] { raise(sig); }, std::chrono::milliseconds(5));

        SigWaiter wait(
            {.sig = sig, .polling_frequency = std::chrono::milliseconds(1), .timeout = std::chrono::milliseconds(100)});

        EXPECT_TRUE(wait.received_signal());
    }
}

} // namespace test
