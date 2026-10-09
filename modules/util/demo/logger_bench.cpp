#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include <fmt/core.h>

#include <sihd/util/ALogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/thread.hpp>

using namespace sihd::util;

SIHD_NEW_LOGGER("bench");

namespace
{
class NullSink: public ALogger
{
    public:
        NullSink(): ALogger("null") {}

        void log(const LogInfo & info, std::string_view msg) override
        {
            (void)info;
            (void)msg;
        }
};

class SlowSink: public ALogger
{
    public:
        SlowSink(): ALogger("slow") {}

        void log(const LogInfo & info, std::string_view msg) override
        {
            (void)info;
            (void)msg;
            const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(10);
            while (std::chrono::steady_clock::now() < end)
            {
            }
        }
};

void reset_loggers()
{
    LoggerManager::set_level(LogLevel::debug);
    LoggerManager::clear_loggers();
    LoggerManager::clear_filters();
}

// async rows include the queue flush
double emit_messages_per_second(int threads, int msgs_per_thread)
{
    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
    {
        pool.emplace_back([msgs_per_thread] {
            for (int i = 0; i < msgs_per_thread; ++i)
            {
                SIHD_LOG(debug, "bench {} payload {}", i, "0123456789");
            }
        });
    }
    for (std::thread & t : pool)
        t.join();
    LoggerManager::clear_loggers();
    const std::chrono::duration<double> secs = std::chrono::steady_clock::now() - start;
    return (threads * msgs_per_thread) / secs.count();
}

} // namespace

int main()
{
    (void)thread::set_name("bench");
    for (int threads : {1, 4})
    {
        reset_loggers();
        LoggerManager::add(new NullSink());
        fmt::print("{:>2} threads {:>12.0f} msgs/s written\n", threads, emit_messages_per_second(threads, 100000));

        reset_loggers();
        LoggerManager::add(new NullSink());
        LoggerManager::set_level(LogLevel::notice);
        fmt::print("{:>2} threads {:>12.0f} msgs/s filtered out (level)\n",
                   threads,
                   emit_messages_per_second(threads, 100000));

        reset_loggers();
        LoggerManager::add(new NullSink());
        LoggerManager::filter(new LoggerFilter({.source_regex = "^bench$"}));
        fmt::print("{:>2} threads {:>12.0f} msgs/s filtered out (regex)\n",
                   threads,
                   emit_messages_per_second(threads, 100000));

        std::FILE *out = std::fopen("/dev/null", "w");
        reset_loggers();
        LoggerManager::add(new LoggerStream(out));
        fmt::print("{:>2} threads {:>12.0f} msgs/s file sink\n", threads, emit_messages_per_second(threads, 100000));
        std::fclose(out);

        out = std::fopen("/dev/null", "w");
        reset_loggers();
        LoggerManager::async(new LoggerStream(out));
        fmt::print("{:>2} threads {:>12.0f} msgs/s file sink, async\n",
                   threads,
                   emit_messages_per_second(threads, 100000));
        std::fclose(out);

        reset_loggers();
        LoggerManager::add(new SlowSink());
        fmt::print("{:>2} threads {:>12.0f} msgs/s slow sink\n", threads, emit_messages_per_second(threads, 20000));

        reset_loggers();
        LoggerManager::async(new SlowSink());
        fmt::print("{:>2} threads {:>12.0f} msgs/s slow sink, async\n",
                   threads,
                   emit_messages_per_second(threads, 20000));
    }
    reset_loggers();
    return 0;
}
