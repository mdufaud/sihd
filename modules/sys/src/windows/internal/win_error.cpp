#include "win_error.hpp"

#include <sihd/sys/os.hpp>

using sihd::util::ErrorCode;

namespace sihd::sys::internal
{

Win32Error::Win32Error(DWORD native): value(native) {}

Win32Error::Win32Error(LSTATUS status): value(static_cast<DWORD>(status)) {}

Win32Error Win32Error::last()
{
    return Win32Error(GetLastError());
}

bool Win32Error::failed() const
{
    return value != ERROR_SUCCESS;
}

std::string format_as(Win32Error error)
{
    return os::error_str(static_cast<int>(error.value));
}

} // namespace sihd::sys::internal

namespace sihd::util
{

ErrorCode ErrorResolver<sihd::sys::internal::Win32Error>::code(sihd::sys::internal::Win32Error native)
{
    switch (native.value)
    {
        case ERROR_SUCCESS:
            return ErrorCode::none;
        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_NAME:
        case ERROR_BAD_PATHNAME:
        case ERROR_UNSUPPORTED_TYPE:
            return ErrorCode::invalid_argument;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
        case ERROR_NOT_READY:
        case ERROR_SERVICE_DOES_NOT_EXIST:
            return ErrorCode::not_found;
        case ERROR_ACCESS_DENIED:
        case ERROR_PRIVILEGE_NOT_HELD:
        case ERROR_WRITE_PROTECT:
        case ERROR_CANCELLED:
        case ERROR_SERVICE_DISABLED:
            return ErrorCode::permission_denied;
        case WAIT_TIMEOUT:
        case ERROR_TIMEOUT:
        case ERROR_SERVICE_REQUEST_TIMEOUT:
            return ErrorCode::timeout;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
        case ERROR_BUSY:
            return ErrorCode::would_block;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return ErrorCode::out_of_memory;
        case ERROR_INVALID_FUNCTION:
        case ERROR_NOT_SUPPORTED:
        case ERROR_CALL_NOT_IMPLEMENTED:
            return ErrorCode::not_supported;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
        case ERROR_SERVICE_EXISTS:
            return ErrorCode::already_exists;
        case ERROR_MORE_DATA:
        case ERROR_INSUFFICIENT_BUFFER:
            return ErrorCode::overflow;
        case ERROR_SERVICE_NOT_ACTIVE:
            return ErrorCode::closed;
        default:
            return ErrorCode::unknown;
    }
}

} // namespace sihd::util
