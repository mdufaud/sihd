#ifndef __SIHD_SYS_WINDOWS_INTERNAL_WIN_ERROR_HPP__
#define __SIHD_SYS_WINDOWS_INTERNAL_WIN_ERROR_HPP__

#include <windows.h>

#include <string>

#include <sihd/util/Error.hpp>

namespace sihd::sys::internal
{

struct Win32Error
{
        explicit Win32Error(DWORD native);
        explicit Win32Error(LSTATUS status);

        static Win32Error last();

        bool failed() const;

        DWORD value;
};

std::string format_as(Win32Error error);

} // namespace sihd::sys::internal

namespace sihd::util
{

template <>
struct ErrorResolver<sihd::sys::internal::Win32Error>
{
        static ErrorCode code(sihd::sys::internal::Win32Error native);
};

} // namespace sihd::util

#endif
