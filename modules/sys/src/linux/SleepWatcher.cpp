#include <string>

#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <atomic>
# include <optional>
# include <utility>

# include <sihd/sys/linux/dbus.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/Worker.hpp>
# include <sihd/util/time.hpp>

# include "internal/login1.hpp"
#endif

#include <sihd/sys/SleepWatcher.hpp>

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

SIHD_LOGGER;

namespace dbus = sihd::sys::linux_::dbus;

namespace
{

constexpr ZStringView delay_reason = "reacts before the system sleeps";

} // namespace

struct SleepWatcher::Impl
{
        ~Impl() { (void)this->stop(); }

        Result<void> take_delay_lock()
        {
            auto lock = internal::login1_inhibit_sleep(connection, app_name, delay_reason, "delay");
            SIHD_UNEXPECTED_RETURN(lock);
            delay_lock = std::move(*lock);
            return {};
        }

        void handle(const dbus::Message & message)
        {
            auto sleeping = dbus::Iter(message).get<bool>();
            if (!sleeping.has_value())
                return;
            if (*sleeping)
            {
                callback(Event::suspending);
                delay_lock.reset();
                return;
            }
            // without the lock the callback still comes, only too late for the next sleep to wait for it
            auto taken = this->take_delay_lock();
            if (!taken.has_value())
                SIHD_LOG(warning, "SleepWatcher: {}", taken.error().message);
            callback(Event::resumed);
        }

        bool run()
        {
            dbus::Message message;
            while (!stopping)
            {
                auto received = message.receive(connection, Duration(time::hours(1)));
                if (!received.has_value())
                {
                    SIHD_LOG(error, "SleepWatcher: {}", received.error().message);
                    return false;
                }
                if (*received && message.member() == "PrepareForSleep")
                    this->handle(message);
            }
            return true;
        }

        Result<void> stop()
        {
            if (!worker.is_worker_started())
                return {};
            stopping = true;
            auto interrupted = connection.interrupt();
            SIHD_UNEXPECTED_RETURN(interrupted);
            worker.stop_worker();
            connection.close();
            delay_lock.reset();
            return {};
        }

        std::string app_name;
        Callback callback;
        dbus::Connection connection;
        std::optional<dbus::UnixFd> delay_lock;
        std::atomic<bool> stopping = false;
        Worker worker;
};

Result<void> SleepWatcher::start(const Callback & callback)
{
    if (!callback)
        return std::unexpected(Error(invalid_argument, "a sleep watcher needs a callback"));
    if (this->is_running())
        return std::unexpected(Error(already_exists, "sleep watcher is already running"));
    // a loop ended by a lost connection left its thread to join
    auto stopped = _impl->stop();
    SIHD_UNEXPECTED_RETURN(stopped);

    auto opened = _impl->connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    auto matched = _impl->connection.add_match(dbus::signal_rule("org.freedesktop.login1",
                                                                 "/org/freedesktop/login1",
                                                                 "org.freedesktop.login1.Manager",
                                                                 "PrepareForSleep"));
    auto taken = matched.has_value() ? _impl->take_delay_lock() : std::move(matched);
    if (!taken.has_value())
        _impl->connection.close();
    SIHD_UNEXPECTED_RETURN(taken);

    _impl->callback = callback;
    _impl->stopping = false;
    _impl->worker.set_method([impl = _impl.get()]() { return impl->run(); });
    if (!_impl->worker.start_sync_worker("sleep-watcher"))
    {
        _impl->connection.close();
        _impl->delay_lock.reset();
        return std::unexpected(Error(unknown, "could not start the sleep watcher thread"));
    }
    return {};
}

Result<void> SleepWatcher::stop()
{
    return _impl->stop();
}

bool SleepWatcher::is_running() const
{
    return _impl->worker.is_worker_running();
}

#else

struct SleepWatcher::Impl
{
        std::string app_name;
};

Result<void> SleepWatcher::start(const Callback & callback)
{
    if (!callback)
        return std::unexpected(Error(invalid_argument, "a sleep watcher needs a callback"));
    return std::unexpected(Error(not_supported, "sleep notification is not supported on this platform"));
}

Result<void> SleepWatcher::stop()
{
    return {};
}

bool SleepWatcher::is_running() const
{
    return false;
}

#endif

SleepWatcher::SleepWatcher(): _impl(std::make_unique<Impl>()) {}

SleepWatcher::~SleepWatcher() = default;

void SleepWatcher::set_app_name(const std::string & name)
{
    _impl->app_name = name;
}

} // namespace sihd::sys
