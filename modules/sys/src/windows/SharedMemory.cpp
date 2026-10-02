#include <windows.h>

#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

std::expected<void, Error> SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    (void)mode; // Windows uses security descriptors, not POSIX mode

    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    HANDLE handle = CreateFileMappingA(INVALID_HANDLE_VALUE, // use paging file
                                       nullptr,              // default security
                                       PAGE_READWRITE,
                                       static_cast<DWORD>(size >> 32),
                                       static_cast<DWORD>(size & 0xFFFFFFFF),
                                       id.data());
    if (handle == nullptr)
        return std::unexpected(Error(io_error, "could not create file mapping '{}': {}", id, os::last_error_str()));

    void *addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (addr == nullptr)
    {
        auto error = Error(io_error, "could not map view of '{}': {}", id, os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }

    // Store handle as fd (cast to int for compatibility with class interface)
    _fd = reinterpret_cast<intptr_t>(handle);
    _addr = addr;
    _created = true;
    _id = id;
    _size = size;

    return {};
}

std::expected<void, Error> SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    (void)mode;

    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    HANDLE handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, id.data());
    if (handle == nullptr)
        return std::unexpected(Error(not_found, "could not open file mapping '{}': {}", id, os::last_error_str()));

    void *addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (addr == nullptr)
    {
        auto error = Error(io_error, "could not map view of '{}': {}", id, os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }

    _fd = reinterpret_cast<intptr_t>(handle);
    _addr = addr;
    _created = false;
    _id = id;
    _size = size;

    return {};
}

std::expected<void, Error> SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    (void)mode;

    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    HANDLE handle = OpenFileMappingA(FILE_MAP_READ, FALSE, id.data());
    if (handle == nullptr)
        return std::unexpected(Error(not_found, "could not open file mapping '{}': {}", id, os::last_error_str()));

    void *addr = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, size);
    if (addr == nullptr)
    {
        auto error = Error(io_error, "could not map view of '{}': {}", id, os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    _fd = reinterpret_cast<intptr_t>(handle);
    _addr = addr;
    _created = false;
    _id = id;
    _size = size;

    return {};
}

std::expected<void, Error> SharedMemory::clear()
{
    bool ret = true;

    if (_addr != nullptr)
    {
        if (!UnmapViewOfFile(_addr))
            ret = false;
        _addr = nullptr;
    }

    if (_fd != -1)
    {
        HANDLE handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(_fd));
        if (!CloseHandle(handle))
            ret = false;
        _fd = -1;
        _id.clear();
    }

    _size = 0;
    _created = false;

    if (!ret)
        return std::unexpected(Error(io_error, "could not release shared memory: {}", os::last_error_str()));
    return {};
}

} // namespace sihd::sys
