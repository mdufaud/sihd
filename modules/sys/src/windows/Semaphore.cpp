#include <windows.h>

#include <sihd/sys/Semaphore.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

#include "internal/wait.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::deadline_ms;

namespace sihd::sys
{

SIHD_LOGGER;

Result<void> Semaphore::create(const std::string & name, unsigned int value, mode_t mode)
{
    (void)mode;

    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    if (value > static_cast<unsigned int>(LONG_MAX))
        return std::unexpected(Error(invalid_argument, "could not create semaphore '{}': value overflows LONG", name));

    HANDLE handle = CreateSemaphoreA(nullptr, static_cast<LONG>(value), LONG_MAX, name.c_str());
    if (handle == nullptr)
        return std::unexpected(Error(io_error, "could not create semaphore '{}': {}", name, os::last_error_str()));

    _sem = handle;
    _created = GetLastError() != ERROR_ALREADY_EXISTS;
    _name = name;
    return {};
}

Result<void> Semaphore::attach(const std::string & name)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    HANDLE handle = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name.c_str());
    if (handle == nullptr)
        return std::unexpected(Error(not_found, "could not open semaphore '{}': {}", name, os::last_error_str()));

    _sem = handle;
    _created = false;
    _name = name;
    return {};
}

Result<void> Semaphore::clear()
{
    bool ret = true;
    if (_sem != nullptr)
    {
        if (!CloseHandle(static_cast<HANDLE>(_sem)))
            ret = false;
        _sem = nullptr;
        _name.clear();
    }
    _created = false;
    if (!ret)
        return std::unexpected(Error(io_error, "could not clear semaphore"));
    return {};
}

bool Semaphore::wait()
{
    return _sem != nullptr && WaitForSingleObject(static_cast<HANDLE>(_sem), INFINITE) == WAIT_OBJECT_0;
}

bool Semaphore::try_wait()
{
    return _sem != nullptr && WaitForSingleObject(static_cast<HANDLE>(_sem), 0) == WAIT_OBJECT_0;
}

bool Semaphore::wait_for(Duration duration)
{
    if (_sem == nullptr)
        return false;

    return WaitForSingleObject(static_cast<HANDLE>(_sem), deadline_ms(duration)) == WAIT_OBJECT_0;
}

bool Semaphore::post()
{
    return _sem != nullptr && ReleaseSemaphore(static_cast<HANDLE>(_sem), 1, nullptr) != 0;
}

Result<int> Semaphore::value() const
{
    return std::unexpected(Error(not_supported, "no semaphore value on windows"));
}

} // namespace sihd::sys
