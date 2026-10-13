#include <windows.h>

#include <sihd/sys/Semaphore.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

std::expected<void, Error> Semaphore::create(std::string_view name, unsigned int value, mode_t mode)
{
    (void)mode;

    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    if (value > static_cast<unsigned int>(LONG_MAX))
        return std::unexpected(Error(invalid_argument, "could not create semaphore '{}': value overflows LONG", name));

    HANDLE handle = CreateSemaphoreA(nullptr, static_cast<LONG>(value), LONG_MAX, name.data());
    if (handle == nullptr)
        return std::unexpected(Error(io_error, "could not create semaphore '{}': {}", name, os::last_error_str()));

    _sem = handle;
    _created = GetLastError() != ERROR_ALREADY_EXISTS;
    _name = name;
    return {};
}

std::expected<void, Error> Semaphore::attach(std::string_view name)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    HANDLE handle = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name.data());
    if (handle == nullptr)
        return std::unexpected(Error(not_found, "could not open semaphore '{}': {}", name, os::last_error_str()));

    _sem = handle;
    _created = false;
    _name = name;
    return {};
}

std::expected<void, Error> Semaphore::clear()
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

    const DWORD milliseconds = static_cast<DWORD>((duration.get() + 999999) / 1000000);
    return WaitForSingleObject(static_cast<HANDLE>(_sem), milliseconds) == WAIT_OBJECT_0;
}

bool Semaphore::post()
{
    return _sem != nullptr && ReleaseSemaphore(static_cast<HANDLE>(_sem), 1, nullptr) != 0;
}

std::expected<int, Error> Semaphore::value() const
{
    return std::unexpected(Error(not_supported, "no semaphore value on windows"));
}

} // namespace sihd::sys
