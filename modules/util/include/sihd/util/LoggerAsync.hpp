#ifndef __SIHD_UTIL_LOGGERASYNC_HPP__
#define __SIHD_UTIL_LOGGERASYNC_HPP__

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <sihd/util/ALogger.hpp>
#include <sihd/util/SafeQueue.hpp>

namespace sihd::util
{

// destruction must not race log() (LoggerManager serializes both), and a target that
// logs back into this wrapper live-locks the drain
class LoggerAsync: public ALogger
{
    public:
        LoggerAsync(ALogger *target, size_t max_queue_size = 8192, std::string source = "sihd::util::logger_async");
        ~LoggerAsync();

        void log(const LogInfo & info, std::string_view msg) override;

        // messages refused and not yet reported by the drain
        size_t dropped() const;

    private:
        struct Item
        {
                Item(const LogInfo & info, std::string_view msg);
                Item(Item && other);

                LogInfo info;
                std::string source;
                std::string thread_name;
                std::string msg;

            private:
                // the LogInfo views dangle once the emitter's thread_local is gone
                void _reseat();
        };

        void _drain();
        void _process(const LogInfo & info, std::string_view msg);
        void _report_drops();
        void _report_dropped(size_t count);

        const std::string _source;
        std::unique_ptr<ALogger> _target;
        const size_t _max_queue_size;
        SafeQueue<Item> _queue;
        std::atomic<size_t> _dropped {0};
        std::jthread _thread;
};

} // namespace sihd::util

#endif
