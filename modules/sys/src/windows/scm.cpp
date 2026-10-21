#include <windows.h>

#include <array>
#include <cstring>
#include <utility>

#include <sihd/sys/windows/scm.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>

#include "internal/wait.hpp"
#include "internal/win_error.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::deadline_ms;
using sihd::sys::internal::Win32Error;

namespace sihd::sys::windows_::scm
{

namespace
{

constexpr DWORD control_access = SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP | SERVICE_CHANGE_CONFIG | DELETE;
constexpr DWORD settled_states = SERVICE_NOTIFY_STOPPED | SERVICE_NOTIFY_RUNNING | SERVICE_NOTIFY_PAUSED;

DWORD native_start_type(StartType start_type)
{
    switch (start_type)
    {
        case StartType::auto_start:
            return SERVICE_AUTO_START;
        case StartType::manual:
            return SERVICE_DEMAND_START;
        case StartType::disabled:
            return SERVICE_DISABLED;
    }
    return SERVICE_DEMAND_START;
}

State state_from_native(DWORD native)
{
    switch (native)
    {
        case SERVICE_START_PENDING:
            return State::start_pending;
        case SERVICE_STOP_PENDING:
            return State::stop_pending;
        case SERVICE_RUNNING:
            return State::running;
        case SERVICE_CONTINUE_PENDING:
            return State::continue_pending;
        case SERVICE_PAUSE_PENDING:
            return State::pause_pending;
        case SERVICE_PAUSED:
            return State::paused;
        default:
            return State::stopped;
    }
}

Result<Status> query_status(SC_HANDLE service)
{
    std::array<BYTE, sizeof(SERVICE_STATUS_PROCESS)> buffer {};
    DWORD needed = 0;
    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, buffer.data(), static_cast<DWORD>(buffer.size()), &needed)
        == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not query service status: {}", error));
    }

    SERVICE_STATUS_PROCESS native {};
    std::memcpy(&native, buffer.data(), sizeof(native));
    return Status {
        .state = state_from_native(native.dwCurrentState),
        .pid = native.dwProcessId,
        .exit_code = native.dwWin32ExitCode,
    };
}

Result<SC_HANDLE> open_native(const std::string & name, DWORD access)
{
    SC_HANDLE manager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (manager == nullptr)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not open service control manager: {}", error));
    }
    Defer close_manager([manager] { CloseServiceHandle(manager); });

    SC_HANDLE service = OpenServiceA(manager, name.c_str(), access);
    if (service == nullptr)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not open service '{}': {}", name, error));
    }
    return service;
}

void CALLBACK on_status_change(void *parameter)
{
    *static_cast<bool *>(static_cast<SERVICE_NOTIFYW *>(parameter)->pContext) = true;
}

Result<void> wait_for_state(const std::string & name, State target, Duration timeout)
{
    auto watched = open_native(name, SERVICE_QUERY_STATUS);
    SIHD_UNEXPECTED_RETURN(watched);

    bool notified = false;
    SERVICE_NOTIFYW notify {};
    notify.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
    notify.pfnNotifyCallback = on_status_change;
    notify.pContext = &notified;
    const Win32Error registered {NotifyServiceStatusChangeW(*watched, settled_states, &notify)};
    if (registered.failed())
    {
        CloseServiceHandle(*watched);
        return std::unexpected(Error(registered, "could not watch service '{}': {}", name, registered));
    }

    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    while (!notified)
    {
        const Duration remaining = timeout.nanoseconds() < 0 ? timeout : deadline - clock.now();
        if (timeout.nanoseconds() >= 0 && remaining.nanoseconds() <= 0)
            break;
        SleepEx(deadline_ms(remaining), TRUE);
    }
    // no APC is queued after the close: one already queued runs here, while notify is alive
    CloseServiceHandle(*watched);
    SleepEx(0, TRUE);

    if (!notified)
        return std::unexpected(Error(ErrorCode::timeout, "service '{}' transition timed out", name));
    const Win32Error status {notify.dwNotificationStatus};
    if (status.failed())
        return std::unexpected(Error(status, "could not watch service '{}': {}", name, status));
    if (state_from_native(notify.ServiceStatus.dwCurrentState) != target)
        return std::unexpected(Error(io_error,
                                     "service '{}' left its transition with exit code {}",
                                     name,
                                     notify.ServiceStatus.dwWin32ExitCode));
    return {};
}

} // namespace

struct Service::Impl
{
        ~Impl() { this->close(); }

        void close()
        {
            if (service != nullptr)
            {
                CloseServiceHandle(service);
                service = nullptr;
            }
            name.clear();
        }

        std::string name;
        SC_HANDLE service = nullptr;
};

Service::Service(): _impl(std::make_unique<Impl>()) {}

Service::Service(Service && other) = default;

Service & Service::operator=(Service && other) = default;

Service::~Service() = default;

Result<void> Service::open(const std::string & name, bool control)
{
    this->close();
    auto service = open_native(name, control ? control_access : SERVICE_QUERY_STATUS);
    SIHD_UNEXPECTED_RETURN(service);
    _impl->name = name;
    _impl->service = *service;
    return {};
}

Result<void> Service::create(const std::string & name, const std::string & bin_path, StartType start_type)
{
    this->close();
    SC_HANDLE manager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (manager == nullptr)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not open service control manager: {}", error));
    }
    Defer close_manager([manager] { CloseServiceHandle(manager); });

    _impl->service = CreateServiceA(manager,
                                    name.c_str(),
                                    name.c_str(),
                                    control_access,
                                    SERVICE_WIN32_OWN_PROCESS,
                                    native_start_type(start_type),
                                    SERVICE_ERROR_NORMAL,
                                    bin_path.c_str(),
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
    if (_impl->service == nullptr)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not create service '{}': {}", name, error));
    }
    _impl->name = name;
    return {};
}

void Service::close()
{
    _impl->close();
}

bool Service::is_open() const
{
    return _impl != nullptr && _impl->service != nullptr;
}

Result<void> Service::start(Duration timeout)
{
    if (StartServiceA(_impl->service, 0, nullptr) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not start service: {}", error));
    }
    return wait_for_state(_impl->name, State::running, timeout);
}

Result<void> Service::stop(Duration timeout)
{
    SERVICE_STATUS status {};
    if (ControlService(_impl->service, SERVICE_CONTROL_STOP, &status) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not stop service: {}", error));
    }
    return wait_for_state(_impl->name, State::stopped, timeout);
}

Result<void> Service::set_start_type(StartType start_type)
{
    if (ChangeServiceConfigA(_impl->service,
                             SERVICE_NO_CHANGE,
                             native_start_type(start_type),
                             SERVICE_NO_CHANGE,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr)
        == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not change service start type: {}", error));
    }
    return {};
}

Result<void> Service::remove()
{
    if (DeleteService(_impl->service) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not remove service: {}", error));
    }
    return {};
}

Result<Status> Service::status() const
{
    return query_status(_impl->service);
}

} // namespace sihd::sys::windows_::scm
