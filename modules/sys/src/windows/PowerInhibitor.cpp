#include <windows.h>

#include <array>
#include <string>
#include <utility>

#include <sihd/sys/PowerInhibitor.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/str.hpp>

#include "internal/win_error.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::Win32Error;

namespace
{

constexpr std::array<POWER_REQUEST_TYPE, 1> sleep_types = {PowerRequestSystemRequired};
// MS doc: the display request alone does not keep the system awake
constexpr std::array<POWER_REQUEST_TYPE, 2> display_types = {PowerRequestDisplayRequired, PowerRequestSystemRequired};

Result<void> release_request(HANDLE & request, ArrayView<POWER_REQUEST_TYPE> types)
{
    if (request == INVALID_HANDLE_VALUE)
        return {};
    Win32Error failure {static_cast<DWORD>(ERROR_SUCCESS)};
    for (const POWER_REQUEST_TYPE type : types)
    {
        if (!PowerClearRequest(request, type))
            failure = Win32Error::last();
    }
    CloseHandle(std::exchange(request, INVALID_HANDLE_VALUE));
    if (failure.failed())
        return std::unexpected(Error(failure, "could not clear power request: {}", failure));
    return {};
}

Result<void> create_request(HANDLE & request, ZStringView reason, ArrayView<POWER_REQUEST_TYPE> types)
{
    std::wstring wide_reason = str::to_wstr(reason);
    REASON_CONTEXT context {};
    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = wide_reason.data();

    HANDLE created = PowerCreateRequest(&context);
    if (created == INVALID_HANDLE_VALUE)
    {
        const Win32Error failure = Win32Error::last();
        return std::unexpected(Error(failure, "could not create power request: {}", failure));
    }
    for (const POWER_REQUEST_TYPE type : types)
    {
        if (!PowerSetRequest(created, type))
        {
            const Win32Error failure = Win32Error::last();
            CloseHandle(created);
            return std::unexpected(Error(failure, "could not set power request: {}", failure));
        }
    }
    request = created;
    return {};
}

} // namespace

struct PowerInhibitor::Impl
{
        ~Impl()
        {
            (void)release_request(sleep_request, sleep_types);
            (void)release_request(display_request, display_types);
        }

        HANDLE sleep_request = INVALID_HANDLE_VALUE;
        HANDLE display_request = INVALID_HANDLE_VALUE;
};

PowerInhibitor::PowerInhibitor(): _impl(std::make_unique<Impl>()) {}

PowerInhibitor::PowerInhibitor(PowerInhibitor && other) = default;

PowerInhibitor & PowerInhibitor::operator=(PowerInhibitor && other) = default;

PowerInhibitor::~PowerInhibitor() = default;

void PowerInhibitor::set_app_name(const std::string &) {}

Result<void> PowerInhibitor::inhibit_sleep(ZStringView reason)
{
    if (this->is_sleep_inhibited())
        return std::unexpected(Error(already_exists, "sleep is already inhibited"));
    return create_request(_impl->sleep_request, reason, sleep_types);
}

Result<void> PowerInhibitor::inhibit_display(ZStringView reason)
{
    if (this->is_display_inhibited())
        return std::unexpected(Error(already_exists, "display is already inhibited"));
    return create_request(_impl->display_request, reason, display_types);
}

Result<void> PowerInhibitor::release()
{
    auto sleep_released = release_request(_impl->sleep_request, sleep_types);
    auto display_released = release_request(_impl->display_request, display_types);
    SIHD_UNEXPECTED_RETURN(sleep_released);
    return display_released;
}

bool PowerInhibitor::is_sleep_inhibited() const
{
    return _impl->sleep_request != INVALID_HANDLE_VALUE;
}

bool PowerInhibitor::is_display_inhibited() const
{
    return _impl->display_request != INVALID_HANDLE_VALUE;
}

} // namespace sihd::sys
