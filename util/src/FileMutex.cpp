#include <sihd/util/Clocks.hpp>
#include <sihd/util/FileMutex.hpp>
#include <sihd/util/platform.hpp>

#include <fmt/format.h>

#if !defined(__SIHD_WINDOWS__)
# include <sys/file.h>
#else
# include <io.h>
# include <windows.h>
#endif

namespace sihd::util
{

namespace
{

bool filemutex_try_lock_for_ex_sh(FileMutex & mutex, Timestamp duration, bool shared)
{
    constexpr time_t retry_in_ms = time::milliseconds(10);

    SteadyClock clock;
    time_t begin = clock.now();
    bool success;

    while (true)
    {
        success = shared ? mutex.try_lock_shared() : mutex.try_lock();
        if (success)
            break;

        time_t diff = clock.now() - begin;
        if (diff > duration)
            break;
        time::nsleep(std::min(retry_in_ms, duration - diff));
    }

    return success;
}

} // namespace

FileMutex::FileMutex(File && file)
{
    _file = std::move(file);
}

FileMutex::FileMutex(std::string_view path, bool create_file_if_not_exist)
{
    _file.open(path, create_file_if_not_exist ? "a" : "r");
}

FileMutex::FileMutex(FileMutex && other)
{
    *this = std::move(other);
}

FileMutex & FileMutex::operator=(FileMutex && other)
{
    this->_file = std::move(other._file);
    return *this;
}

File & FileMutex::file()
{
    return _file;
}

void FileMutex::lock()
{
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
#if !defined(__SIHD_WINDOWS__)
    flock(_file.fd(), LOCK_EX);
#else
    // https://stackoverflow.com/questions/70812957/what-is-the-windows-equivalent-of-unixs-flockh-lock-sh

    // LOCK_SH = 0, LOCK_EX = LOCKFILE_EXCLUSIVE_LOCK, LOCK_NB = LOCKFILE_FAIL_IMMEDIATELY
    HANDLE handle = (HANDLE)_get_osfhandle(_file.fd());
    const DWORD allBitsSet = ~DWORD(0);
    _OVERLAPPED overlapped = {0};
    if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, allBitsSet, allBitsSet, &overlapped))
    {
        throw std::runtime_error("LockFileEx failed with error " + std::to_string(GetLastError()));
    }
#endif
}

bool FileMutex::try_lock()
{
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
#if !defined(__SIHD_WINDOWS__)
    return flock(_file.fd(), LOCK_EX | LOCK_NB) == 0;
#else
    HANDLE handle = (HANDLE)_get_osfhandle(_file.fd());
    const DWORD allBitsSet = ~DWORD(0);
    _OVERLAPPED overlapped = {0};
    return LockFileEx(handle,
                      LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                      0,
                      allBitsSet,
                      allBitsSet,
                      &overlapped);
#endif
}

void FileMutex::unlock()
{
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
#if !defined(__SIHD_WINDOWS__)
    flock(_file.fd(), LOCK_UN);
#else
    HANDLE handle = (HANDLE)_get_osfhandle(_file.fd());
    const DWORD allBitsSet = ~DWORD(0);
    _OVERLAPPED overlapped = {0};
    if (!UnlockFileEx(handle, 0, allBitsSet, allBitsSet, &overlapped))
    {
        throw std::runtime_error("UnlockFileEx failed with error " + std::to_string(GetLastError()));
    }
#endif
}

void FileMutex::lock_shared()
{
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
#if !defined(__SIHD_WINDOWS__)
    flock(_file.fd(), LOCK_SH);
#else
    HANDLE handle = (HANDLE)_get_osfhandle(_file.fd());
    const DWORD allBitsSet = ~DWORD(0);
    _OVERLAPPED overlapped = {0};
    if (!LockFileEx(handle, 0, 0, allBitsSet, allBitsSet, &overlapped))
    {
        throw std::runtime_error("LockFileEx failed with error " + std::to_string(GetLastError()));
    }
#endif
}

bool FileMutex::try_lock_shared()
{
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
#if !defined(__SIHD_WINDOWS__)
    return flock(_file.fd(), LOCK_SH | LOCK_NB) == 0;
#else
    HANDLE handle = (HANDLE)_get_osfhandle(_file.fd());
    const DWORD allBitsSet = ~DWORD(0);
    _OVERLAPPED overlapped = {0};
    return LockFileEx(handle, LOCKFILE_FAIL_IMMEDIATELY, 0, allBitsSet, allBitsSet, &overlapped);
#endif
}

void FileMutex::unlock_shared()
{
    this->unlock();
}

bool FileMutex::try_lock_for(Timestamp duration)
{
    constexpr bool shared = false;
    return filemutex_try_lock_for_ex_sh(*this, duration, shared);
}

bool FileMutex::try_lock_shared_for(Timestamp duration)
{
    constexpr bool shared = true;
    return filemutex_try_lock_for_ex_sh(*this, duration, shared);
}

} // namespace sihd::util
