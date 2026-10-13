#include <fcntl.h> // O_*
#include <semaphore.h>

#include <cerrno>

#include <sihd/sys/Semaphore.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>

#include "internal/deadline.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

#if defined(__SIHD_UNIX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_APPLE__) && !defined(__SIHD_EMSCRIPTEN__)

std::expected<void, Error> Semaphore::create(std::string_view name, unsigned int value, mode_t mode)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    sem_t *sem = sem_open(name.data(), O_CREAT | O_EXCL, mode, value);
    if (sem == SEM_FAILED)
        return std::unexpected(Error::from_errno("could not sem_open '{}'", name));

    _sem = sem;
    _created = true;
    _name = name;
    return {};
}

std::expected<void, Error> Semaphore::attach(std::string_view name)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    sem_t *sem = sem_open(name.data(), 0);
    if (sem == SEM_FAILED)
        return std::unexpected(Error::from_errno("could not sem_open '{}'", name));

    _sem = sem;
    _created = false;
    _name = name;
    return {};
}

std::expected<void, Error> Semaphore::clear()
{
    bool ret = true;
    if (_sem != nullptr)
    {
        if (_created && sem_unlink(_name.c_str()) == -1)
            ret = false;
        if (sem_close(static_cast<sem_t *>(_sem)) == -1)
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
    while (_sem != nullptr && sem_wait(static_cast<sem_t *>(_sem)) == -1)
    {
        if (errno != EINTR)
            return false;
    }
    return _sem != nullptr;
}

bool Semaphore::try_wait()
{
    return _sem != nullptr && sem_trywait(static_cast<sem_t *>(_sem)) == 0;
}

bool Semaphore::wait_for(Duration duration)
{
    const Timestamp begin = internal::steady_now();
    while (_sem != nullptr)
    {
        auto deadline = internal::realtime_deadline(begin, duration);
        if (!deadline)
            return false;
        if (sem_timedwait(static_cast<sem_t *>(_sem), &*deadline) == 0)
            return true;
        if (errno != EINTR)
            return false;
    }
    return false;
}

bool Semaphore::post()
{
    return _sem != nullptr && sem_post(static_cast<sem_t *>(_sem)) == 0;
}

std::expected<int, Error> Semaphore::value() const
{
    if (_sem == nullptr)
        return std::unexpected(Error(not_initialized, "semaphore '{}' is not attached", _name));

    int value = 0;
    if (sem_getvalue(static_cast<sem_t *>(_sem), &value) == -1)
        return std::unexpected(Error::from_errno("could not sem_getvalue '{}'", _name));
    return value;
}

#else

// no POSIX named semaphores (android bionic, apple lacks sem_timedwait/sem_getvalue, emscripten)

std::expected<void, Error> Semaphore::create(std::string_view, unsigned int, mode_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> Semaphore::attach(std::string_view)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> Semaphore::clear()
{
    return {};
}

bool Semaphore::wait()
{
    return false;
}

bool Semaphore::try_wait()
{
    return false;
}

bool Semaphore::wait_for(Duration)
{
    return false;
}

bool Semaphore::post()
{
    return false;
}

std::expected<int, Error> Semaphore::value() const
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

#endif

} // namespace sihd::sys
