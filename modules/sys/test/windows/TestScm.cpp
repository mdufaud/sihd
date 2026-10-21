#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/os.hpp>
#include <sihd/sys/windows/scm.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../test_helper.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::windows_;

class TestScm: public ::testing::Test
{
    protected:
        TestScm() { LoggerManager::stream(); }

        virtual ~TestScm() { LoggerManager::clear_loggers(); }
};

class TestScmService: public TestScm
{
    protected:
        void SetUp() override
        {
            // a stopping service stays marked for delete: each test needs a fresh name
            _name = fmt::format("sihd_test_{}_{}", os::pid(), SteadyClock().now().nanoseconds());
            auto created = _service.create(_name, this->command(), scm::StartType::manual);
            if (!created && created.error().code == ErrorCode::permission_denied)
                GTEST_SKIP() << created.error().message;
            ASSERT_TRUE(created.has_value()) << created.error().message;
        }

        void TearDown() override
        {
            if (!_service.is_open())
                return;
            (void)_service.stop(Duration(time::sec(5)));
            (void)_service.remove();
        }

        virtual std::string command() const
        {
            return fmt::format("\"{}\" {}", bin_path("sihd_sys_test_service"), _name);
        }

        std::string _name;
        scm::Service _service;
};

class TestScmSlowStop: public TestScmService
{
    protected:
        std::string command() const override
        {
            return fmt::format("\"{}\" {} slow-stop", bin_path("sihd_sys_test_service"), _name);
        }
};

class TestScmMissingBinary: public TestScmService
{
    protected:
        std::string command() const override { return fmt::format("\"{}\"", bin_path("sihd_missing_service")); }
};

TEST_F(TestScm, test_query)
{
    if (os::is_run_by_wine())
        GTEST_SKIP() << "wine starts RpcSs on demand";
    scm::Service service;
    auto opened = service.open("RpcSs");
    ASSERT_TRUE(opened.has_value()) << opened.error().message;

    auto status = service.status();

    ASSERT_TRUE(status.has_value()) << status.error().message;
    EXPECT_EQ(status->state, scm::State::running);
    EXPECT_NE(status->pid, 0u);
}

TEST_F(TestScm, test_missing)
{
    scm::Service service;

    auto opened = service.open("sihd_missing_service");

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
    EXPECT_FALSE(service.is_open());
}

TEST_F(TestScmService, test_start_stop)
{
    auto initial = _service.status();
    ASSERT_TRUE(initial.has_value()) << initial.error().message;
    EXPECT_EQ(initial->state, scm::State::stopped);

    auto started = _service.start(Duration(time::sec(10)));
    ASSERT_TRUE(started.has_value()) << started.error().message;
    auto running = _service.status();
    ASSERT_TRUE(running.has_value()) << running.error().message;
    EXPECT_EQ(running->state, scm::State::running);
    EXPECT_NE(running->pid, 0u);

    auto stopped = _service.stop(Duration(time::sec(10)));
    ASSERT_TRUE(stopped.has_value()) << stopped.error().message;
    auto final_status = _service.status();
    ASSERT_TRUE(final_status.has_value()) << final_status.error().message;
    EXPECT_EQ(final_status->state, scm::State::stopped);
    EXPECT_EQ(final_status->exit_code, 0u);
}

TEST_F(TestScmService, test_stop_not_running)
{
    auto stopped = _service.stop(Duration(time::sec(1)));

    ASSERT_FALSE(stopped.has_value());
    EXPECT_EQ(stopped.error().code, ErrorCode::closed);
}

TEST_F(TestScmService, test_start_disabled)
{
    ASSERT_TRUE(_service.set_start_type(scm::StartType::disabled).has_value());

    auto started = _service.start(Duration(time::sec(1)));

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::permission_denied);
}

TEST_F(TestScmService, test_remove)
{
    ASSERT_TRUE(_service.remove().has_value());
    _service.close();

    scm::Service removed;
    auto opened = removed.open(_name);

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
}

TEST_F(TestScmSlowStop, test_stop_timeout)
{
    ASSERT_TRUE(_service.start(Duration(time::sec(10))).has_value());

    auto stopped = _service.stop(Duration(time::milli(100)));

    ASSERT_FALSE(stopped.has_value());
    EXPECT_EQ(stopped.error().code, ErrorCode::timeout);
    auto status = _service.status();
    ASSERT_TRUE(status.has_value()) << status.error().message;
    EXPECT_EQ(status->state, scm::State::stop_pending);
}

TEST_F(TestScmMissingBinary, test_start_missing_binary)
{
    auto started = _service.start(Duration(time::sec(1)));

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::not_found) << started.error().message;
}

} // namespace test
