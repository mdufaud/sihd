#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/windows/Job.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../test_helper.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::windows_;

class TestJob: public ::testing::Test
{
    protected:
        TestJob() { LoggerManager::stream(); }

        virtual ~TestJob() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            auto created = _job.create();
            ASSERT_TRUE(created.has_value()) << created.error().message;
        }

        virtual void TearDown() { (void)_job.terminate(); }

        Job _job;
        Process _process {std::vector<std::string> {helper_path(), "sleep", "10"}};
};

TEST_F(TestJob, test_new_process_event)
{
    ASSERT_TRUE(_process.execute());

    auto assigned = _job.assign(_process);
    ASSERT_TRUE(assigned.has_value()) << assigned.error().message;

    auto event = _job.wait_event(Duration(time::sec(5)));
    ASSERT_TRUE(event.has_value()) << event.error().message;
    EXPECT_EQ(event->kind, Job::Event::Kind::new_process);
    EXPECT_EQ(event->process_id, _process.pid());
}

TEST_F(TestJob, test_terminate_events)
{
    if (os::is_run_by_wine())
        GTEST_SKIP() << "wine posts no exit_process message on TerminateJobObject";
    ASSERT_TRUE(_process.execute());
    ASSERT_TRUE(_job.assign(_process).has_value());
    ASSERT_TRUE(_job.wait_event(Duration(time::sec(5))).has_value());

    ASSERT_TRUE(_job.terminate().has_value());

    auto exited = _job.wait_event(Duration(time::sec(5)));
    ASSERT_TRUE(exited.has_value()) << exited.error().message;
    EXPECT_EQ(exited->kind, Job::Event::Kind::exit_process);
    EXPECT_EQ(exited->process_id, _process.pid());
    auto emptied = _job.wait_event(Duration(time::sec(5)));
    ASSERT_TRUE(emptied.has_value()) << emptied.error().message;
    EXPECT_EQ(emptied->kind, Job::Event::Kind::active_process_zero);
    EXPECT_TRUE(_process.wait_exit(5000).value_or(false));
}

TEST_F(TestJob, test_accounting)
{
    if (os::is_run_by_wine())
        GTEST_SKIP() << "wine does not implement job accounting";
    ASSERT_TRUE(_process.execute());
    ASSERT_TRUE(_job.assign(_process).has_value());

    auto accounting = _job.accounting();

    ASSERT_TRUE(accounting.has_value()) << accounting.error().message;
    EXPECT_EQ(accounting->active_processes, 1u);
    EXPECT_EQ(accounting->total_processes, 1u);
}

TEST_F(TestJob, test_kill_on_close)
{
    ASSERT_TRUE(_job.set_limits({}).has_value());
    ASSERT_TRUE(_process.execute());
    ASSERT_TRUE(_job.assign(_process).has_value());

    _job.close();

    EXPECT_FALSE(_job.is_open());
    EXPECT_TRUE(_process.wait_exit(5000).value_or(false));
}

TEST_F(TestJob, test_limits)
{
    Job::Limits limits;
    limits.process_memory_bytes = 1ull << 30;
    limits.job_memory_bytes = 2ull << 30;
    limits.per_process_cpu_time = Duration(time::sec(60));

    auto set = _job.set_limits(limits);

    EXPECT_TRUE(set.has_value()) << set.error().message;
}

TEST_F(TestJob, test_wait_timeout)
{
    auto event = _job.wait_event(Duration(time::milli(50)));

    ASSERT_FALSE(event.has_value());
    EXPECT_EQ(event.error().code, ErrorCode::timeout);
}

} // namespace test
