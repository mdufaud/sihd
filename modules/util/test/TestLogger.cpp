#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <sihd/util/ALogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerAsync.hpp>
#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/LoggerThrow.hpp>
#include <sihd/util/Waitable.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
class LogCounter: public ALogger
{
    public:
        LogCounter(): ALogger("counter") {}

        ~LogCounter() = default;

        int emergency = 0;
        int alert = 0;
        int critical = 0;
        int debug = 0;
        int info = 0;
        int notice = 0;
        int warning = 0;
        int error = 0;

        std::string msg;
        std::string src;

        virtual void log(const LogInfo & info, std::string_view msg) override
        {
            this->msg = msg;
            this->src = info.source;
            switch (info.level)
            {
                case LogLevel::emergency:
                    ++this->emergency;
                    break;
                case LogLevel::alert:
                    ++this->alert;
                    break;
                case LogLevel::critical:
                    ++this->critical;
                    break;
                case LogLevel::error:
                    ++this->error;
                    break;
                case LogLevel::warning:
                    ++this->warning;
                    break;
                case LogLevel::notice:
                    ++this->notice;
                    break;
                case LogLevel::info:
                    ++this->info;
                    break;
                case LogLevel::debug:
                    ++this->debug;
                    break;
                default:
                    break;
            }
        };
};

class AlwaysFilter: public ILoggerFilter
{
    public:
        bool filter(const LogInfo & info) override
        {
            (void)info;
            return true;
        }

        bool filter(const LogInfo & info, std::string_view msg) override
        {
            (void)info;
            (void)msg;
            return false;
        }
};

class RefuseWarnings: public ILoggerFilter
{
    public:
        bool filter(const LogInfo & info) override { return info.level == LogLevel::warning; }

        bool filter(const LogInfo & info, std::string_view msg) override
        {
            (void)info;
            (void)msg;
            return false;
        }
};

class SharedStateSink: public ALogger
{
    public:
        struct State
        {
                std::atomic<int> count = 0;
                std::string last;
                std::mutex mutex;
        };

        SharedStateSink(State *state): ALogger("shared-state"), _state(state) {}

        void log(const LogInfo & info, std::string_view msg) override
        {
            (void)info;
            _state->count.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> l(_state->mutex);
            _state->last = std::string(msg);
        }

    private:
        State *_state;
};

class BlockingSink: public ALogger
{
    public:
        BlockingSink(std::atomic<int> *logged, std::string *drop_reports):
            ALogger("blocking"),
            _logged(logged),
            _drop_reports(drop_reports)
        {
        }

        void log(const LogInfo & info, std::string_view msg) override
        {
            if (info.source != "test::async")
            {
                if (info.level == LogLevel::warning)
                    *_drop_reports = std::string(msg);
                return;
            }
            _logged->fetch_add(1, std::memory_order_relaxed);
            _waitable.wait_guard([this] { return _released.load(std::memory_order_relaxed); });
        }

        void release()
        {
            _released.store(true, std::memory_order_relaxed);
            _waitable.notify_all();
        }

    private:
        std::atomic<int> *_logged;
        std::string *_drop_reports;
        std::atomic<bool> _released = false;
        Waitable _waitable;
};

class LoggingBackSink: public ALogger
{
    public:
        LoggingBackSink(): ALogger("logging-back") {}

        std::atomic<int> logged = 0;

        void log(const LogInfo & info, std::string_view msg) override
        {
            (void)msg;
            ++logged;
            // the log-back is dropped at the manager gate: no recursion
            SIHD_LOG_LVL(info.level, "logs back from a sink");
        }
};

class TestLogger: public ::testing::Test
{
    protected:
        TestLogger() = default;
        ;
        virtual ~TestLogger() = default;
        ;

        LogCounter *log_counter = nullptr;

        virtual void SetUp()
        {
            _old_thread_name = thread::name();
            (void)thread::set_name("main");

            this->log_counter = new LogCounter();
            LoggerManager::add(this->log_counter);
        }

        virtual void TearDown()
        {
            (void)thread::set_name(_old_thread_name);

            LoggerManager::set_level(LogLevel::debug);
            LoggerManager::clear_loggers();
            LoggerManager::clear_filters();
        }

        bool has_logged_every_levels()
        {
            return log_counter->emergency > 0 && log_counter->alert > 0 && log_counter->critical > 0
                   && log_counter->error > 0 && log_counter->warning > 0 && log_counter->notice > 0
                   && log_counter->info > 0 && log_counter->debug > 0;
        }

        std::string _old_thread_name;
};

TEST_F(TestLogger, test_logger_basic)
{
    Logger log("test::logger");

    LoggerManager::add(new LoggerStream());
    log.log(sihd::util::LogLevel::info, "test");
    ASSERT_EQ(log_counter->src, "test::logger");
    ASSERT_EQ(log_counter->msg, "test");
    log.debug("debug");
    ASSERT_EQ(log_counter->msg, "debug");
    log.info("info");
    ASSERT_EQ(log_counter->msg, "info");
    log.warning("warning");
    ASSERT_EQ(log_counter->msg, "warning");
    log.error("error");
    ASSERT_EQ(log_counter->msg, "error");
    log.critical("critical");
    ASSERT_EQ(log_counter->msg, "critical");
    log.alert("alert");
    ASSERT_EQ(log_counter->msg, "alert");
    log.notice("notice");
    ASSERT_EQ(log_counter->msg, "notice");
    log.emergency("emergency");
    ASSERT_TRUE(this->has_logged_every_levels());
}

TEST_F(TestLogger, test_logger_every_levels)
{
    Logger log("test::logger");
    log.emergency("emergency");
    ASSERT_EQ(log_counter->msg, "emergency");
    log.alert("alert");
    ASSERT_EQ(log_counter->msg, "alert");
    log.critical("critical");
    log.error("error");
    log.warning("warning");
    log.notice("notice");
    log.info("info");
    log.debug("debug");
    ASSERT_TRUE(this->has_logged_every_levels());
    ASSERT_EQ(log_counter->emergency, 1);
    ASSERT_EQ(log_counter->alert, 1);
    ASSERT_EQ(log_counter->critical, 1);
    ASSERT_EQ(log_counter->error, 1);
    ASSERT_EQ(log_counter->warning, 1);
    ASSERT_EQ(log_counter->notice, 1);
    ASSERT_EQ(log_counter->info, 1);
    ASSERT_EQ(log_counter->debug, 1);
}

TEST_F(TestLogger, test_logger_variadic)
{
    Logger log("test::logger");
    LoggerManager::add(new LoggerStream());
    log.log(LogLevel::info, "int: {} - str: {}", 42, "hello");
    ASSERT_EQ(log_counter->msg, "int: 42 - str: hello");
    log.debug("fmt: {:02}", 3);
    ASSERT_EQ(log_counter->msg, "fmt: 03");
    log.warning("no arg");
    ASSERT_EQ(log_counter->msg, "no arg");
    log.emergency("{}", "emergency");
    ASSERT_EQ(log_counter->msg, "emergency");
}

TEST_F(TestLogger, test_logger_macros)
{
    LoggerManager::add(new LoggerStream());
    SIHD_LOG(debug, "DEBUG");
    SIHD_LOG(info, "INFO");
    SIHD_LOG(warning, "WARNING");
    SIHD_LOG(error, "ERROR");
    SIHD_LOG(critical, "CRITICAL");
    SIHD_LOG(alert, "ALERT");
    SIHD_LOG(notice, "NOTICE");
    SIHD_LOG(emergency, "EMERGENCY");
    ASSERT_TRUE(this->has_logged_every_levels());
    ASSERT_EQ(log_counter->src, "test");

    SIHD_TRACEL("TEST TRACE");
    ASSERT_EQ(log_counter->debug, 2);

    SIHD_LOG(info, "fmt test: {} - {}", 1.23, "hello");
    ASSERT_EQ(log_counter->msg, "fmt test: 1.23 - hello");

    SIHD_LOG(info, "int test: {:02} - {}", 2, "world");
    ASSERT_EQ(log_counter->msg, "int test: 02 - world");
    ASSERT_EQ(log_counter->info, 3);
}

TEST_F(TestLogger, test_logger_expected)
{
    std::expected<int, Error> ok = 42;
    ASSERT_FALSE(SIHD_UNEXPECTED_LOG(ok));

    std::expected<int, Error> err = std::unexpected(Error(invalid_argument, "oops"));
    ASSERT_TRUE(SIHD_UNEXPECTED_LOG(err));
    ASSERT_EQ(log_counter->src, "test");
    ASSERT_TRUE(log_counter->msg.starts_with("TestLogger:"));
    ASSERT_TRUE(log_counter->msg.ends_with(": oops"));

    Logger local_logger("test");
    log_unexpected_error(local_logger, err.error(), std::source_location {});
    ASSERT_EQ(log_counter->msg, "oops");
}

TEST_F(TestLogger, test_logger_format)
{
    const std::string source = "test::format";
    LogInfo info(source, LogLevel::warning);

    const std::string line = info.format("the message");
    ASSERT_EQ(line,
              fmt::format("{}.{:09}\t[{}]\t{:<9} {}\t{}\n",
                          info.timespec.tv_sec,
                          info.timespec.tv_nsec,
                          info.thread_name,
                          info.strlevel,
                          source,
                          "the message"));

    const std::string line_tid = info.format("the message", true);
    ASSERT_EQ(line_tid,
              fmt::format("{}.{:09}\t{}\t[{}]\t{:<9} {}\t{}\n",
                          info.timespec.tv_sec,
                          info.timespec.tv_nsec,
                          info.thread_id_str,
                          info.thread_name,
                          info.strlevel,
                          source,
                          "the message"));
}

TEST_F(TestLogger, test_logger_should_log)
{
    LoggerManager::filter(new LoggerFilter({.level_lower = LogLevel::warning}));
    ASSERT_FALSE(LoggerManager::should_log("test", LogLevel::info));
    SIHD_LOG(info, "dropped before formatting");
    ASSERT_EQ(log_counter->info, 0);
    SIHD_LOG(warning, "counted");
    ASSERT_EQ(log_counter->warning, 1);
    LoggerManager::clear_filters();

    log_counter->add_filter(new LoggerFilter({.level_lower = LogLevel::error}));
    ASSERT_FALSE(LoggerManager::should_log("test", LogLevel::info));
    LogCounter *loose = new LogCounter();
    LoggerManager::add(loose);
    ASSERT_TRUE(LoggerManager::should_log("test", LogLevel::info));
    SIHD_LOG(info, "reaches the loose sink only");
    ASSERT_EQ(log_counter->info, 0);
    ASSERT_EQ(loose->info, 1);
    LoggerManager::rm(loose);
    delete loose;
    ASSERT_FALSE(LoggerManager::should_log("test", LogLevel::info));
    log_counter->delete_filters();

    LoggerManager::filter(new LoggerFilter({.message_regex = ".*not.*"}));
    ASSERT_TRUE(LoggerManager::should_log("test", LogLevel::info));
    SIHD_LOG(info, "Should not count");
    ASSERT_EQ(log_counter->info, 0);
    SIHD_LOG(info, "Should count");
    ASSERT_EQ(log_counter->info, 1);
}

TEST_F(TestLogger, test_logger_filter_message)
{
    LoggerManager::filter(new LoggerFilter({
        .message_regex = ".*not.*",
    }));
    SIHD_LOG(info, "Should count");
    EXPECT_EQ(log_counter->info, 1);
    SIHD_LOG(info, "Should not count");
    EXPECT_EQ(log_counter->info, 1);

    LoggerManager::filter(new LoggerFilter({
        .message_regex = "^Hello.*",
    }));
    SIHD_LOG(info, "Hello world");
    EXPECT_EQ(log_counter->info, 1);
    SIHD_LOG(info, "hello world");
    EXPECT_EQ(log_counter->info, 2);
}

TEST_F(TestLogger, test_logger_filter_thread)
{
    // test filter all threads except main
    LoggerManager::filter(new LoggerFilter({
        .thread_ne = thread::id(),
    }));

    SIHD_LOG(info, "Should count");
    EXPECT_EQ(log_counter->info, 1);

    std::jthread thread1([]() { SIHD_LOG(warning, "Should not count"); });
    thread1.join();
    EXPECT_EQ(log_counter->warning, 0);

    LoggerManager::clear_filters();

    // test filter main thread id

    LoggerManager::filter(new LoggerFilter({
        .thread_eq = thread::id(),
    }));
    SIHD_LOG(error, "Should not count");
    EXPECT_EQ(log_counter->error, 0);

    std::jthread thread2([]() { SIHD_LOG(critical, "Should count"); });
    thread2.join();
    EXPECT_EQ(log_counter->critical, 1);

    LoggerManager::clear_filters();

    // test filter thread named main or titi

    LoggerManager::filter(new LoggerFilter({
        .thread_regex = "^(main|titi)$",
    }));
    SIHD_LOG(debug, "Should not count");
    EXPECT_EQ(log_counter->debug, 0);

    std::jthread thread3([]() {
        (void)thread::set_name("toto");
        SIHD_LOG(debug, "Should count");
        (void)thread::set_name("titi");
        SIHD_LOG(debug, "Should not count");
    });
    thread3.join();
    EXPECT_EQ(log_counter->debug, 1);
}

TEST_F(TestLogger, test_logger_filter_source)
{
    LoggerManager::filter(new LoggerFilter({
        .source_regex = "^test",
    }));
    SIHD_LOG(critical, "Should not count");
    EXPECT_EQ(log_counter->critical, 0);
    LoggerManager::clear_filters();

    LoggerManager::filter(new LoggerFilter({
        .source_regex = "other",
    }));
    SIHD_LOG(debug, "Should count");
    EXPECT_EQ(log_counter->debug, 1);
}

TEST_F(TestLogger, test_logger_filter_level)
{
    log_counter->add_filter(new LoggerFilter({
        .level_lower = LogLevel::warning,
    }));
    SIHD_LOG(error, "Should count");
    EXPECT_EQ(log_counter->error, 1);
    SIHD_LOG(warning, "Should count");
    EXPECT_EQ(log_counter->warning, 1);
    SIHD_LOG(info, "Should not count");
    EXPECT_EQ(log_counter->info, 0);
    SIHD_LOG(debug, "Should not count");
    EXPECT_EQ(log_counter->debug, 0);
    log_counter->delete_filters();

    LoggerManager::filter(new LoggerFilter({
        .level_lower = LogLevel::critical,
    }));
    SIHD_LOG(critical, "Should count");
    EXPECT_EQ(log_counter->critical, 1);
    SIHD_LOG(error, "Should not count");
    EXPECT_EQ(log_counter->error, 1);
}

TEST_F(TestLogger, test_logger_filter_level_eq_higher)
{
    log_counter->add_filter(new LoggerFilter({
        .level_eq = LogLevel::warning,
    }));
    SIHD_LOG(warning, "Should not count");
    SIHD_LOG(info, "Should count");
    EXPECT_EQ(log_counter->warning, 0);
    EXPECT_EQ(log_counter->info, 1);
    log_counter->delete_filters();

    log_counter->add_filter(new LoggerFilter({
        .level_higher = LogLevel::info,
    }));
    SIHD_LOG(critical, "Should not count");
    SIHD_LOG(debug, "Should count");
    EXPECT_EQ(log_counter->critical, 0);
    EXPECT_EQ(log_counter->debug, 1);
    log_counter->delete_filters();
}

TEST_F(TestLogger, test_logger_remove_filter_type)
{
    log_counter->add_filter(new LoggerFilter({.level_lower = LogLevel::warning}));
    log_counter->add_filter(new AlwaysFilter());
    ASSERT_TRUE(log_counter->remove_filter_type<LoggerFilter>());
    ASSERT_FALSE(log_counter->remove_filter_type<LoggerFilter>());

    SIHD_LOG(info, "still dropped by the remaining filter");
    ASSERT_EQ(log_counter->info, 0);
    log_counter->delete_filters();
    SIHD_LOG(info, "Should count");
    ASSERT_EQ(log_counter->info, 1);
}

TEST_F(TestLogger, test_logger_delete_loggers_type)
{
    LoggerStream *stream = new LoggerStream();
    LoggerManager::add(stream);
    LoggerManager::add(new LogCounter());
    ASSERT_TRUE(LoggerManager::get()->delete_loggers_type<LoggerStream>());
    ASSERT_FALSE(LoggerManager::get()->delete_loggers_type<LoggerStream>());
    ASSERT_FALSE(LoggerManager::get()->has_logger(stream));
    ASSERT_TRUE(LoggerManager::get()->has_logger(log_counter));
    ASSERT_TRUE(LoggerManager::get()->delete_loggers_type<LogCounter>());
    SIHD_LOG(info, "no sink left");
}

TEST_F(TestLogger, test_logger_threads)
{
    std::vector<std::jthread> pool;
    for (int t = 0; t < 4; ++t)
    {
        pool.emplace_back([] {
            for (int i = 0; i < 250; ++i)
            {
                SIHD_LOG(info, "thread log {}", i);
            }
        });
    }
    for (std::jthread & t : pool)
        t.join();
    ASSERT_EQ(log_counter->info, 1000);
}

TEST_F(TestLogger, test_logger_throw)
{
    LoggerManager::thrower();
    Logger log("test::throw");
    EXPECT_THROW(log.error("thrown away"), LoggerThrow::Exception);
    // the throwing sink sits after the counter: the message went through
    ASSERT_EQ(log_counter->error, 1);
    LoggerManager::get()->delete_loggers_type<LoggerThrow>();
    log.error("counted");
    ASSERT_EQ(log_counter->error, 2);
}

TEST_F(TestLogger, test_logger_async)
{
    SharedStateSink::State state;
    LoggerManager::async(new SharedStateSink(&state));
    Logger log("test::async");
    for (int i = 0; i < 100; ++i)
        log.info("{}", i);
    LoggerManager::clear_loggers();
    ASSERT_EQ(state.count.load(), 100);
    ASSERT_EQ(state.last, "99");
}

TEST_F(TestLogger, test_logger_async_threads)
{
    SharedStateSink::State state;
    LoggerManager::async(new SharedStateSink(&state));
    std::vector<std::jthread> pool;
    for (int t = 0; t < 4; ++t)
    {
        pool.emplace_back([] {
            for (int i = 0; i < 50; ++i)
            {
                SIHD_LOG(info, "async thread log {}", i);
            }
        });
    }
    for (std::jthread & t : pool)
        t.join();
    LoggerManager::clear_loggers();
    ASSERT_EQ(state.count.load(), 200);
}

TEST_F(TestLogger, test_logger_async_max_queue)
{
    std::atomic<int> logged = 0;
    std::string drop_reports;
    BlockingSink *sink = new BlockingSink(&logged, &drop_reports);
    LoggerManager::async(sink, 2, std::nullopt, "test::async-report");
    Logger log("test::async");
    for (int i = 0; i < 10; ++i)
        log.info("{}", i);
    sink->release();
    LoggerManager::clear_loggers();
    // one blocked in the target, at most the queue capacity waiting behind it
    ASSERT_GE(logged.load(), 2);
    ASSERT_LE(logged.load(), 3);
    ASSERT_NE(drop_reports.find("messages dropped"), std::string::npos);
}

TEST_F(TestLogger, test_logger_async_options)
{
    SharedStateSink::State state;
    LoggerManager::async(new SharedStateSink(&state), 8192, LoggerFilter::Options {.level_lower = LogLevel::warning});
    Logger log("test::async");
    log.info("held back by the async wrapper bound");
    log.warning("queued");
    LoggerManager::clear_loggers();
    ASSERT_EQ(state.count.load(), 1);
    ASSERT_EQ(state.last, "queued");
}

TEST_F(TestLogger, test_logger_async_target_bound)
{
    ASSERT_TRUE(LoggerManager::get()->delete_loggers_type<LogCounter>());
    this->log_counter = nullptr;

    SharedStateSink::State state;
    SharedStateSink *target = new SharedStateSink(&state);
    target->add_filter(new LoggerFilter({.level_lower = LogLevel::error}));
    LoggerManager::async(target);
    SIHD_LOG(info, "refused by the target");
    LoggerManager::clear_loggers();
    ASSERT_EQ(state.count.load(), 0);
}

TEST_F(TestLogger, test_logger_async_target_filter_change)
{
    SharedStateSink::State state;
    {
        SharedStateSink *target = new SharedStateSink(&state);
        target->add_filter(new LoggerFilter({.level_lower = LogLevel::error}));
        LoggerAsync async(target);
        const std::string source = "test::async";
        async.log(LogInfo(source, LogLevel::info), "refused by the target");
    }
    ASSERT_EQ(state.count.load(), 0);

    {
        SharedStateSink *target = new SharedStateSink(&state);
        LoggerAsync async(target);
        const std::string source = "test::async";
        async.log(LogInfo(source, LogLevel::info), "accepted without the filter");
    }
    ASSERT_EQ(state.count.load(), 1);
}

TEST_F(TestLogger, test_logger_async_direct_flush)
{
    SharedStateSink::State state;
    {
        LoggerAsync async(new SharedStateSink(&state));
        const std::string source = "test::async";
        LogInfo info(source, LogLevel::info);
        for (int i = 0; i < 10; ++i)
            async.log(info, std::to_string(i));
    }
    ASSERT_EQ(state.count.load(), 10);
    ASSERT_EQ(state.last, "9");
}

TEST_F(TestLogger, test_logger_async_drop_report_unfiltered)
{
    std::atomic<int> logged = 0;
    std::string drop_reports;
    {
        BlockingSink *sink = new BlockingSink(&logged, &drop_reports);
        sink->add_filter(new RefuseWarnings());
        LoggerAsync async(sink, 1, "test::async-drops");
        const std::string source = "test::async";
        LogInfo info(source, LogLevel::info);
        while (async.dropped() == 0)
            async.log(info, "overflow");
        sink->release();
    }
    ASSERT_GE(logged.load(), 1);
    ASSERT_NE(drop_reports.find("messages dropped"), std::string::npos);
}

TEST_F(TestLogger, test_logger_reentrant_dropped)
{
    LoggingBackSink *sink = new LoggingBackSink();
    LoggerManager::add(sink);
    SIHD_LOG(info, "outer message");
    ASSERT_EQ(sink->logged.load(), 1);
    ASSERT_EQ(log_counter->info, 1);
}

TEST_F(TestLogger, test_logger_set_level)
{
    LoggerManager::set_level(LogLevel::warning);

    ASSERT_FALSE(LoggerManager::should_log_level(LogLevel::info));
    ASSERT_TRUE(LoggerManager::should_log_level(LogLevel::warning));
    ASSERT_FALSE(LoggerManager::should_log("test::level", LogLevel::debug));

    SIHD_LOG(info, "dropped before formatting");
    SIHD_LOG(warning, "at the bound");
    ASSERT_EQ(log_counter->info, 0);
    ASSERT_EQ(log_counter->warning, 1);

    LoggerManager::set_level(LogLevel::debug);
    SIHD_LOG(info, "level restored");
    ASSERT_EQ(log_counter->info, 1);
}

} // namespace test
