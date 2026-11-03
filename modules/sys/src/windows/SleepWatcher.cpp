// clang-format off
#include <windows.h>
#include <powrprof.h>
// clang-format on

#include <string>

#include <sihd/sys/SleepWatcher.hpp>

#include "internal/win_error.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::Win32Error;

namespace
{

// mingw-w64 declares the registration function, not the callback flavor of its parameters
constexpr DWORD device_notify_callback = 2;

using NotifyRoutine = ULONG(CALLBACK *)(PVOID context, ULONG type, PVOID setting);

struct NotifyParameters
{
        NotifyRoutine routine;
        PVOID context;
};

} // namespace

struct SleepWatcher::Impl
{
        ~Impl() { (void)this->stop(); }

        // each resume comes as PBT_APMRESUMEAUTOMATIC, PBT_APMRESUMESUSPEND only follows on user input
        static ULONG CALLBACK on_power_event(PVOID context, ULONG type, PVOID)
        {
            Impl *impl = static_cast<Impl *>(context);
            if (type == PBT_APMSUSPEND)
                impl->callback(Event::suspending);
            else if (type == PBT_APMRESUMEAUTOMATIC)
                impl->callback(Event::resumed);
            return ERROR_SUCCESS;
        }

        Result<void> stop()
        {
            if (registration == nullptr)
                return {};
            const Win32Error unregistered {PowerUnregisterSuspendResumeNotification(registration)};
            if (unregistered.failed())
                return std::unexpected(Error(unregistered, "could not stop the sleep watcher: {}", unregistered));
            registration = nullptr;
            return {};
        }

        Callback callback;
        NotifyParameters parameters {};
        HPOWERNOTIFY registration = nullptr;
};

SleepWatcher::SleepWatcher(): _impl(std::make_unique<Impl>()) {}

SleepWatcher::~SleepWatcher() = default;

void SleepWatcher::set_app_name(const std::string &) {}

Result<void> SleepWatcher::start(const Callback & callback)
{
    if (!callback)
        return std::unexpected(Error(invalid_argument, "a sleep watcher needs a callback"));
    if (this->is_running())
        return std::unexpected(Error(already_exists, "sleep watcher is already running"));

    _impl->callback = callback;
    _impl->parameters = NotifyParameters {&Impl::on_power_event, _impl.get()};
    const Win32Error registered {PowerRegisterSuspendResumeNotification(device_notify_callback,
                                                                        static_cast<HANDLE>(&_impl->parameters),
                                                                        &_impl->registration)};
    if (registered.failed())
    {
        _impl->registration = nullptr;
        return std::unexpected(Error(registered, "could not start the sleep watcher: {}", registered));
    }
    return {};
}

Result<void> SleepWatcher::stop()
{
    return _impl->stop();
}

bool SleepWatcher::is_running() const
{
    return _impl->registration != nullptr;
}

} // namespace sihd::sys
