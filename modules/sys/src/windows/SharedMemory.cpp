#include <windows.h>

#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

bool SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    (void)mode; // Windows uses security descriptors, not POSIX mode

    this->clear();

    HANDLE handle = CreateFileMappingA(INVALID_HANDLE_VALUE, // use paging file
                                       nullptr,              // default security
                                       PAGE_READWRITE,
                                       static_cast<DWORD>(size >> 32),
                                       static_cast<DWORD>(size & 0xFFFFFFFF),
                                       id.data());
    if (handle == nullptr)
    {
        SIHD_LOG(error, "SharedMemory: CreateFileMappingA: {}", os::last_error_str());
        return false;
    }

    void *addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (addr == nullptr)
    {
        SIHD_LOG(error, "SharedMemory: MapViewOfFile: {}", os::last_error_str());
        CloseHandle(handle);
        return false;
    }

    // Store handle as fd (cast to int for compatibility with class interface)
    _fd = reinterpret_cast<intptr_t>(handle);
    _addr = addr;
    _created = true;
    _id = id;
    _size = size;

    return true;
}

bool SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    (void)mode;

    this->clear();

    HANDLE handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, id.data());
    if (handle == nullptr)
    {
        SIHD_LOG(error, "SharedMemory: OpenFileMappingA: {}", os::last_error_str());
        return false;
    }

    void *addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (addr == nullptr)
    {
        SIHD_LOG(error, "SharedMemory: MapViewOfFile: {}", os::last_error_str());
        CloseHandle(handle);
        return false;
    }

    _fd = reinterpret_cast<intptr_t>(handle);
    _addr = addr;
    _created = false;
    _id = id;
    _size = size;

    return true;
}

bool SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    (void)mode;

    this->clear();

    HANDLE handle = OpenFileMappingA(FILE_MAP_READ, FALSE, id.data());
    if (handle == nullptr)
    {
        SIHD_LOG(error, "SharedMemory: OpenFileMappingA: {}", os::last_error_str());
        return false;
    }

    void *addr = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, size);
    if (addr == nullptr)
    {
        SIHD_LOG(error, "SharedMemory: MapViewOfFile: {}", os::last_error_str());
        CloseHandle(handle);
        return false;
    }

    _fd = reinterpret_cast<intptr_t>(handle);
    _addr = addr;
    _created = false;
    _id = id;
    _size = size;

    return true;
}

bool SharedMemory::clear()
{
    bool ret = true;

    if (_addr != nullptr)
    {
        if (!UnmapViewOfFile(_addr))
        {
            SIHD_LOG(error, "SharedMemory: UnmapViewOfFile: {}", os::last_error_str());
            ret = false;
        }
        _addr = nullptr;
    }

    if (_fd != -1)
    {
        HANDLE handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(_fd));
        if (!CloseHandle(handle))
        {
            SIHD_LOG(error, "SharedMemory: CloseHandle: {}", os::last_error_str());
            ret = false;
        }
        _fd = -1;
        _id.clear();
    }

    _size = 0;
    _created = false;

    return ret;
}

} // namespace sihd::sys
