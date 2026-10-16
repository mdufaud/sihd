#include <algorithm>
#include <chrono>

#include <gtest/gtest.h>

#include <sihd/sys/ProcessMonitor.hpp>
#include <sihd/sys/info.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestProcessMonitor: public ::testing::Test
{
    protected:
        TestProcessMonitor() { LoggerManager::stream(); }

        virtual ~TestProcessMonitor() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!ProcessMonitor::supported)
            {
                GTEST_SKIP() << "process monitoring not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestProcessMonitor, test_process_monitor_poll)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "proc metrics are not faithful under qemu";

    ProcessMonitor monitor;

    ASSERT_TRUE(monitor.poll());
    EXPECT_FALSE(monitor.samples().empty());

    auto self = monitor.sample(getpid());
    ASSERT_TRUE(self.has_value());
    EXPECT_EQ(self->metrics.pid, getpid());
    EXPECT_GT(self->metrics.rss_bytes, 0u);
    EXPECT_GE(self->metrics.thread_count, 1u);
    EXPECT_NE(self->metrics.state, ProcessState::unknown);
    EXPECT_EQ(self->cpu_percent, 0.0);

    EXPECT_FALSE(monitor.sample(-1).has_value());
}

TEST_F(TestProcessMonitor, test_process_monitor_cpu)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "cpu accounting is not faithful under qemu";

    ProcessMonitor monitor;
    ASSERT_TRUE(monitor.poll());

    const auto burn_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < burn_until)
    {
    }

    ASSERT_TRUE(monitor.poll());
    auto self = monitor.sample(getpid());
    ASSERT_TRUE(self.has_value());
    const double cores = std::max<uint32_t>(info::cpu_count(), 1);
    EXPECT_GT(self->cpu_percent, 0.0);
    EXPECT_LE(self->cpu_percent, 2.0 * 100.0 / cores);
}

TEST_F(TestProcessMonitor, test_process_monitor_start_stop)
{
    int ticks = 0;
    Handler<ProcessMonitor *> handler([&ticks](ProcessMonitor *) { ++ticks; });

    ProcessMonitor monitor;
    ASSERT_TRUE(monitor.add_observer(&handler));
    ASSERT_TRUE(monitor.set_frequency(20.0));
    ASSERT_TRUE(monitor.start());
    ASSERT_TRUE(monitor.is_running());

    time::msleep(300);

    ASSERT_TRUE(monitor.stop());
    EXPECT_FALSE(monitor.is_running());
    EXPECT_GE(ticks, 1);
    EXPECT_FALSE(monitor.samples().empty());
}

TEST_F(TestProcessMonitor, test_process_monitor_name_filter)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "proc metrics are not faithful under qemu";

    ProcessMonitor monitor;
    ASSERT_TRUE(monitor.set_name_filter("sihd_sys.*"));

    ASSERT_TRUE(monitor.poll());
    auto samples = monitor.samples();
    ASSERT_FALSE(samples.empty());
    EXPECT_TRUE(monitor.sample(getpid()).has_value());
    for (const ProcessSample & sample : samples)
    {
        EXPECT_NE(sample.name.find("sihd_sys"), std::string::npos) << sample.name;
    }
}

} // namespace test
