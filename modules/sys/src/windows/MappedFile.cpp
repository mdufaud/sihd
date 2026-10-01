#include <windows.h>

#include <sihd/sys/MappedFile.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
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

std::expected<Mapping, Error> open_map(std::string_view path, DWORD desired_access, DWORD protect, DWORD view_access)
{
    HANDLE handle = CreateFileA(path.data(),
                                desired_access,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    LARGE_INTEGER li;
    if (!GetFileSizeEx(handle, &li))
    {
        auto error = Error::from_errno("could not get size of '{}'", path);
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    if (li.QuadPart <= 0)
    {
        CloseHandle(handle);
        return std::unexpected(Error(invalid_argument, "cannot map an empty file '{}'", path));
    }
    // a zero size maps the whole current file
    HANDLE mapping = CreateFileMappingA(handle, nullptr, protect, 0, 0, nullptr);
    if (mapping == nullptr)
    {
        auto error = Error(io_error, "could not create file mapping: {}", os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    void *addr = MapViewOfFile(mapping, view_access, 0, 0, 0);
    if (addr == nullptr)
    {
        auto error = Error(io_error, "could not map view of file: {}", os::last_error_str());
        CloseHandle(mapping);
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    return Mapping {static_cast<int>(reinterpret_cast<intptr_t>(handle)),
                    static_cast<int>(reinterpret_cast<intptr_t>(mapping)),
                    addr,
                    static_cast<size_t>(li.QuadPart)};
}

} // namespace

std::expected<void, Error> MappedFile::create(std::string_view path, size_t size, mode_t mode)
{
    (void)mode;
    auto cleared = this->clear();
    if (!cleared)
        return cleared;
    if (size == 0)
        return std::unexpected(Error(invalid_argument, "cannot map an empty size"));
    HANDLE handle = CreateFileA(path.data(),
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return std::unexpected(Error::from_errno("could not create '{}'", path));
    // commit the file size now, a bare mapping would extend it lazily
    LARGE_INTEGER offset {};
    offset.QuadPart = static_cast<LONGLONG>(size);
    if (SetFilePointerEx(handle, offset, nullptr, FILE_BEGIN) == 0 || SetEndOfFile(handle) == 0)
    {
        auto error = Error::from_errno("could not size '{}'", path);
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    HANDLE mapping = CreateFileMappingA(handle,
                                        nullptr,
                                        PAGE_READWRITE,
                                        static_cast<DWORD>(size >> 32),
                                        static_cast<DWORD>(size & 0xFFFFFFFF),
                                        nullptr);
    if (mapping == nullptr)
    {
        auto error = Error(io_error, "could not create file mapping: {}", os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    void *addr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (addr == nullptr)
    {
        auto error = Error(io_error, "could not map view of file: {}", os::last_error_str());
        CloseHandle(mapping);
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    _fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
    _mapping = static_cast<int>(reinterpret_cast<intptr_t>(mapping));
    _addr = addr;
    _size = size;
    _read_only = false;
    _path = path;
    return {};
}

std::expected<void, Error> MappedFile::open_read_only(std::string_view path)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;
    auto mapped = open_map(path, GENERIC_READ, PAGE_READONLY, FILE_MAP_READ);
    SIHD_UNEXPECTED_RETURN(mapped);
    _fd = mapped->fd;
    _mapping = mapped->mapping;
    _addr = mapped->addr;
    _size = mapped->size;
    _read_only = true;
    _path = path;
    return {};
}

std::expected<void, Error> MappedFile::open_read_write(std::string_view path)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;
    auto mapped = open_map(path, GENERIC_READ | GENERIC_WRITE, PAGE_READWRITE, FILE_MAP_ALL_ACCESS);
    SIHD_UNEXPECTED_RETURN(mapped);
    _fd = mapped->fd;
    _mapping = mapped->mapping;
    _addr = mapped->addr;
    _size = mapped->size;
    _read_only = false;
    _path = path;
    return {};
}

std::expected<void, Error> MappedFile::clear()
{
    bool ret = true;
    if (_addr != nullptr)
    {
        if (!UnmapViewOfFile(_addr))
            ret = false;
        _addr = nullptr;
    }
    if (_mapping != -1)
    {
        if (!CloseHandle(int_to_handle(_mapping)))
            ret = false;
        _mapping = -1;
    }
    if (_fd != -1)
    {
        if (!CloseHandle(int_to_handle(_fd)))
            ret = false;
        _fd = -1;
    }
    _size = 0;
    _read_only = false;
    _path.clear();
    if (!ret)
        return std::unexpected(Error(io_error, "could not release mapping: {}", os::last_error_str()));
    return {};
}

std::expected<void, Error> MappedFile::sync(bool async)
{
    if (_addr == nullptr)
        return std::unexpected(Error(not_initialized, "no mapping"));
    if (!FlushViewOfFile(_addr, 0))
        return std::unexpected(Error(io_error, "could not flush view: {}", os::last_error_str()));
    // the file handle is read-only when opened read-only: FlushFileBuffers would fail
    if (!async && !_read_only && !FlushFileBuffers(int_to_handle(_fd)))
        return std::unexpected(Error(io_error, "could not flush file buffers: {}", os::last_error_str()));
    return {};
}

} // namespace sihd::sys
