#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

// signalfd is only available on Linux (not Android, not Emscripten)
#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define SIHD_HAS_SIGNALFD 1
#endif

// other POSIX targets (macOS/BSD/Android) use a dedicated sigwait thread instead of polling
#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_EMSCRIPTEN__) && !defined(SIHD_HAS_SIGNALFD)
# define SIHD_HAS_SIGWAIT 1
#endif

#if defined(SIHD_HAS_SIGNALFD) || defined(SIHD_HAS_SIGWAIT)
# define SIHD_HAS_SIGSET 1
#endif

#if defined(SIHD_HAS_SIGNALFD)
# include <sys/signalfd.h>
# include <unistd.h>
#endif

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

SigWatcher::SigWatcher(const std::string & name, Node *parent):
    Named(name, parent),
    _running(false),
    _polling_interval_ns(time::sec(1)) // Default 1 second for polling mode
{
#if defined(SIHD_HAS_SIGSET)
    sigemptyset(&_sigset);
#endif
#if defined(SIHD_HAS_SIGNALFD)
    _signalfd = -1;
#endif

    this->add_conf("polling_frequency", &SigWatcher::set_polling_frequency);
}

SigWatcher::SigWatcher(std::initializer_list<int> signals, Callback callback): SigWatcher("sig-watcher", nullptr)
{
    _callback = std::move(callback);

    for (int sig : signals)
        this->add_signal(sig);

    this->start();
}

SigWatcher::~SigWatcher()
{
    if (this->is_running())
        this->stop();
}

bool SigWatcher::set_polling_frequency(double frequency)
{
    if (frequency <= 0)
        return false;
    _polling_interval_ns = time::freq(frequency);
    return true;
}

bool SigWatcher::is_running() const
{
    return _running.load(std::memory_order_relaxed);
}

bool SigWatcher::add_signal(int sig)
{
    std::lock_guard l(_mutex);
    auto & sig_controller = _sig_controllers.emplace_back();

    bool success = sig_controller.sig_handler.handle(sig);
    if (success)
    {
        const auto status = signal::status(sig);
        success = status.has_value();
        if (success)
        {
            sig_controller.last_received = status->received.load(std::memory_order_relaxed);
        }
    }

    if (success)
    {
        _signals.emplace_back(sig);
#if defined(SIHD_HAS_SIGSET)
        sigaddset(&_sigset, sig);
#endif
    }
    else
    {
        _sig_controllers.pop_back();
    }

    return success;
}

bool SigWatcher::add_signals(std::span<const int> signals)
{
    return std::all_of(signals.begin(), signals.end(), [this](int sig) { return this->add_signal(sig); });
}

bool SigWatcher::rm_signal(int sig)
{
    std::lock_guard l(_mutex);
#if defined(SIHD_HAS_SIGSET)
    sigdelset(&_sigset, sig);
#endif
    return container::erase_if(_sig_controllers,
                               [sig](const auto & sig_controller) { return sig_controller.sig_handler.sig() == sig; })
           && container::erase(_signals, sig);
}

bool SigWatcher::rm_signals(std::span<const int> signals)
{
    return std::all_of(signals.begin(), signals.end(), [this](int sig) { return this->rm_signal(sig); });
}

bool SigWatcher::call_previous_handler(int sig)
{
    std::lock_guard l(_mutex);
    const auto it = container::find_if(_sig_controllers, [sig](const auto & sig_controller) {
        return sig_controller.sig_handler.sig() == sig;
    });
    const bool found = it != _sig_controllers.end();
    if (found)
        it->sig_handler.call_previous_handler();
    return found;
}

bool SigWatcher::start()
{
    if (_running.load(std::memory_order_relaxed))
        return false;

    _running.store(true, std::memory_order_relaxed);

#if defined(SIHD_HAS_SIGSET)
    if (!signal::block_thread(_signals))
    {
        SIHD_LOG(error, "SigWatcher: blocking signals failed");
        _running.store(false, std::memory_order_relaxed);
        return false;
    }
#endif

#if defined(SIHD_HAS_SIGNALFD)
    _signalfd = signalfd(-1, &_sigset, SFD_NONBLOCK | SFD_CLOEXEC);
    if (_signalfd == -1)
    {
        SIHD_LOG(error, "SigWatcher: signalfd creation failed");
        signal::unblock_thread(_signals);
        _running.store(false, std::memory_order_relaxed);
        return false;
    }

    _worker.set_method([this]() {
        this->_run_signalfd_loop();
        return true;
    });
#elif defined(SIHD_HAS_SIGWAIT)
    // Dedicated sigwait thread for POSIX targets without signalfd (macOS/BSD/Android)
    _worker.set_method([this]() {
        this->_run_sigwait_loop();
        return true;
    });
#else
    // Fallback to polling mode for Windows and Emscripten
    _worker.set_method([this]() {
        this->_run_polling_loop();
        return true;
    });
#endif

    return _worker.start_worker("sigwatcher");
}

bool SigWatcher::stop()
{
    _running.store(false, std::memory_order_relaxed);
    bool ret = _worker.stop_worker();

#if defined(SIHD_HAS_SIGNALFD)
    if (_signalfd >= 0)
    {
        ::close(_signalfd);
        _signalfd = -1;
    }
#endif

#if defined(SIHD_HAS_SIGSET)
    signal::unblock_thread(_signals);
#endif

    return ret;
}

void SigWatcher::_notify_signals()
{
    if (!_signals_to_handle.empty())
    {
        if (_callback)
        {
            for (int sig : _signals_to_handle)
                _callback(sig);
        }
        this->notify_observers(this);
        _signals_to_handle.clear();
    }
}

#if defined(SIHD_HAS_SIGNALFD)

void SigWatcher::_run_signalfd_loop()
{
    thread::set_name("sigwatcher");

    _poll.set_limit(1);
    _poll.set_read_fd(_signalfd);

    while (_running.load(std::memory_order_relaxed))
    {
        int ret = _poll.poll(time::to_ms(_polling_interval_ns.load()));

        if (ret < 0)
        {
            if (_poll.polling_error())
            {
                SIHD_LOG(error, "SigWatcher: poll failed");
                break;
            }
            continue;
        }

        if (ret == 0 || _poll.polling_timeout())
            continue; // Timeout, check _running flag

        for (const auto & event : _poll.events())
        {
            if (event.fd == _signalfd && event.readable)
            {
                struct signalfd_siginfo siginfo;
                ssize_t s = read(_signalfd, &siginfo, sizeof(siginfo));

                if (s != sizeof(siginfo))
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;
                    SIHD_LOG(error, "SigWatcher: signalfd read failed");
                    break;
                }

                {
                    std::lock_guard l(_mutex);
                    const int sig = static_cast<int>(siginfo.ssi_signo);

                    auto it_sigcontroller = container::find_if(_sig_controllers, [sig](const auto & sig_controller) {
                        return sig_controller.sig_handler.sig() == sig;
                    });

                    if (it_sigcontroller != _sig_controllers.end())
                    {
                        // With signalfd, signals are blocked and delivered via fd
                        // The signal handler is never called, so we directly add to signals_to_handle
                        it_sigcontroller->last_received++;
                        _signals_to_handle.emplace_back(sig);
                    }
                }

                this->_notify_signals();
            }
        }
    }

    _poll.clear_fd(_signalfd);
}

#endif

#if defined(SIHD_HAS_SIGWAIT)

void SigWatcher::_run_sigwait_loop()
{
    thread::set_name("sigwatcher");

    while (_running.load(std::memory_order_relaxed))
    {
        const Duration interval = _polling_interval_ns.load(std::memory_order_relaxed);
        struct timespec ts = {
            static_cast<time_t>(interval.nanoseconds() / time::sec(1)),
            static_cast<long>(interval.nanoseconds() % time::sec(1)),
        };

        siginfo_t info;
        int sig = sigtimedwait(&_sigset, &info, &ts);

        if (sig < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue; // timeout or interrupted, re-check _running flag
            SIHD_LOG(error, "SigWatcher: sigtimedwait failed");
            break;
        }

        {
            std::lock_guard l(_mutex);

            auto it_sigcontroller = container::find_if(_sig_controllers, [sig](const auto & sig_controller) {
                return sig_controller.sig_handler.sig() == sig;
            });

            if (it_sigcontroller != _sig_controllers.end())
            {
                // signals are blocked and consumed via sigtimedwait, the async handler never fires
                it_sigcontroller->last_received++;
                _signals_to_handle.emplace_back(sig);
            }
        }

        this->_notify_signals();
    }
}

#endif

void SigWatcher::_run_polling_loop()
{
    thread::set_name("sigwatcher");

    while (_running.load(std::memory_order_relaxed))
    {
        {
            std::lock_guard l(_mutex);
            for (int sig : _signals)
            {
                auto it_sigcontroller = container::find_if(_sig_controllers, [sig](const auto & sig_controller) {
                    return sig_controller.sig_handler.sig() == sig;
                });

                const auto status = signal::status(sig);
                if (status.has_value())
                {
                    const size_t current_received = status->received.load(std::memory_order_relaxed);
                    if (current_received != it_sigcontroller->last_received)
                    {
                        it_sigcontroller->last_received = current_received;
                        _signals_to_handle.emplace_back(sig);
                    }
                }
            }
        }

        this->_notify_signals();

        // Sleep for polling interval
        std::this_thread::sleep_for(std::chrono::nanoseconds(_polling_interval_ns.load(std::memory_order_relaxed)));
    }
}

} // namespace sihd::sys

#undef SIHD_HAS_SIGNALFD
#undef SIHD_HAS_SIGWAIT
#undef SIHD_HAS_SIGSET
