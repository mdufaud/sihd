#include <windows.h>

#include <sihd/sys/MappedFile.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

namespace
{

struct Mapping
{
        int fd = -1;
        int mapping = -1;
        void *addr = nullptr;
        size_t size = 0;
};

HANDLE int_to_handle(int fd)
{
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd));
}

std::optional<Mapping> open_map(std::string_view path, DWORD desired_access, DWORD protect, DWORD view_access)
{
    HANDLE handle = CreateFileA(path.data(),
                                desired_access,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        SIHD_LOG(error, "MappedFile: CreateFileA: {}", os::last_error_str());
        return std::nullopt;
    }
    LARGE_INTEGER li;
    if (!GetFileSizeEx(handle, &li))
    {
        SIHD_LOG(error, "MappedFile: GetFileSizeEx: {}", os::last_error_str());
        CloseHandle(handle);
        return std::nullopt;
    }
    if (li.QuadPart <= 0)
    {
        SIHD_LOG(error, "MappedFile: cannot map an empty file: {}", path);
        CloseHandle(handle);
        return std::nullopt;
    }
    // a zero size maps the whole current file
    HANDLE mapping = CreateFileMappingA(handle, nullptr, protect, 0, 0, nullptr);
    if (mapping == nullptr)
    {
        SIHD_LOG(error, "MappedFile: CreateFileMappingA: {}", os::last_error_str());
        CloseHandle(handle);
        return std::nullopt;
    }
    void *addr = MapViewOfFile(mapping, view_access, 0, 0, 0);
    if (addr == nullptr)
    {
        SIHD_LOG(error, "MappedFile: MapViewOfFile: {}", os::last_error_str());
        CloseHandle(mapping);
        CloseHandle(handle);
        return std::nullopt;
    }
    return Mapping {static_cast<int>(reinterpret_cast<intptr_t>(handle)),
                    static_cast<int>(reinterpret_cast<intptr_t>(mapping)),
                    addr,
                    static_cast<size_t>(li.QuadPart)};
}

} // namespace

bool MappedFile::create(std::string_view path, size_t size, mode_t mode)
{
    (void)mode;
    this->clear();
    if (size == 0)
    {
        SIHD_LOG(error, "MappedFile: cannot map an empty size");
        return false;
    }
    HANDLE handle = CreateFileA(path.data(),
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        SIHD_LOG(error, "MappedFile: CreateFileA: {}", os::last_error_str());
        return false;
    }
    // commit the file size now, a bare mapping would extend it lazily
    LARGE_INTEGER offset {};
    offset.QuadPart = static_cast<LONGLONG>(size);
    if (SetFilePointerEx(handle, offset, nullptr, FILE_BEGIN) == 0 || SetEndOfFile(handle) == 0)
    {
        SIHD_LOG(error, "MappedFile: SetEndOfFile: {}", os::last_error_str());
        CloseHandle(handle);
        return false;
    }
    HANDLE mapping = CreateFileMappingA(handle,
                                        nullptr,
                                        PAGE_READWRITE,
                                        static_cast<DWORD>(size >> 32),
                                        static_cast<DWORD>(size & 0xFFFFFFFF),
                                        nullptr);
    if (mapping == nullptr)
    {
        SIHD_LOG(error, "MappedFile: CreateFileMappingA: {}", os::last_error_str());
        CloseHandle(handle);
        return false;
    }
    void *addr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (addr == nullptr)
    {
        SIHD_LOG(error, "MappedFile: MapViewOfFile: {}", os::last_error_str());
        CloseHandle(mapping);
        CloseHandle(handle);
        return false;
    }
    _fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
    _mapping = static_cast<int>(reinterpret_cast<intptr_t>(mapping));
    _addr = addr;
    _size = size;
    _read_only = false;
    _path = path;
    return true;
}

bool MappedFile::open_read_only(std::string_view path)
{
    this->clear();
    auto opt = open_map(path, GENERIC_READ, PAGE_READONLY, FILE_MAP_READ);
    if (!opt)
        return false;
    _fd = opt->fd;
    _mapping = opt->mapping;
    _addr = opt->addr;
    _size = opt->size;
    _read_only = true;
    _path = path;
    return true;
}

bool MappedFile::open_read_write(std::string_view path)
{
    this->clear();
    auto opt = open_map(path, GENERIC_READ | GENERIC_WRITE, PAGE_READWRITE, FILE_MAP_ALL_ACCESS);
    if (!opt)
        return false;
    _fd = opt->fd;
    _mapping = opt->mapping;
    _addr = opt->addr;
    _size = opt->size;
    _read_only = false;
    _path = path;
    return true;
}

bool MappedFile::clear()
{
    bool ret = true;
    if (_addr != nullptr)
    {
        if (!UnmapViewOfFile(_addr))
        {
            SIHD_LOG(error, "MappedFile: UnmapViewOfFile: {}", os::last_error_str());
            ret = false;
        }
        _addr = nullptr;
    }
    if (_mapping != -1)
    {
        if (!CloseHandle(int_to_handle(_mapping)))
        {
            SIHD_LOG(error, "MappedFile: CloseHandle: {}", os::last_error_str());
            ret = false;
        }
        _mapping = -1;
    }
    if (_fd != -1)
    {
        if (!CloseHandle(int_to_handle(_fd)))
        {
            SIHD_LOG(error, "MappedFile: CloseHandle: {}", os::last_error_str());
            ret = false;
        }
        _fd = -1;
    }
    _size = 0;
    _read_only = false;
    _path.clear();
    return ret;
}

bool MappedFile::sync(bool async)
{
    if (_addr == nullptr)
        return false;
    if (!FlushViewOfFile(_addr, 0))
    {
        SIHD_LOG(error, "MappedFile: FlushViewOfFile: {}", os::last_error_str());
        return false;
    }
    // the file handle is read-only when opened read-only: FlushFileBuffers would fail
    if (!async && !_read_only && !FlushFileBuffers(int_to_handle(_fd)))
    {
        SIHD_LOG(error, "MappedFile: FlushFileBuffers: {}", os::last_error_str());
        return false;
    }
    return true;
}

} // namespace sihd::sys
