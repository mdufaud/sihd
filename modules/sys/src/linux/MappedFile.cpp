#include <fcntl.h>    // O_*
#include <sys/mman.h> // mmap, munmap, msync
#include <sys/stat.h>
#include <unistd.h> // close, ftruncate

#include <sihd/sys/MappedFile.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

#if defined(__SIHD_UNIX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

struct Mapping
{
        int fd = -1;
        void *addr = nullptr;
        size_t size = 0;
};

std::optional<Mapping> map_fd(int fd, size_t size, int prot)
{
    void *addr = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        SIHD_LOG(error, "MappedFile: mmap: {}", os::last_error_str());
        return std::nullopt;
    }
    return Mapping {fd, addr, size};
}

std::optional<Mapping> open_map(std::string_view path, int open_flags, int prot)
{
    int fd = ::open(path.data(), open_flags);
    if (fd < 0)
    {
        SIHD_LOG(error, "MappedFile: open: {}", os::last_error_str());
        return std::nullopt;
    }
    struct stat s;
    if (fstat(fd, &s) == -1)
    {
        SIHD_LOG(error, "MappedFile: fstat: {}", os::last_error_str());
        ::close(fd);
        return std::nullopt;
    }
    if (s.st_size <= 0)
    {
        SIHD_LOG(error, "MappedFile: cannot map an empty file: {}", path);
        ::close(fd);
        return std::nullopt;
    }
    return map_fd(fd, (size_t)s.st_size, prot);
}

} // namespace

bool MappedFile::create(std::string_view path, size_t size, mode_t mode)
{
    this->clear();
    if (size == 0)
    {
        SIHD_LOG(error, "MappedFile: cannot map an empty size");
        return false;
    }
    int fd = ::open(path.data(), O_RDWR | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
    {
        SIHD_LOG(error, "MappedFile: open: {}", os::last_error_str());
        return false;
    }
    if (ftruncate(fd, size) == -1)
    {
        SIHD_LOG(error, "MappedFile: ftruncate: {}", os::last_error_str());
        ::close(fd);
        return false;
    }
    auto opt = map_fd(fd, size, PROT_READ | PROT_WRITE);
    if (!opt)
    {
        ::close(fd);
        return false;
    }
    _fd = opt->fd;
    _addr = opt->addr;
    _size = opt->size;
    _read_only = false;
    _path = path;
    return true;
}

bool MappedFile::open_read_only(std::string_view path)
{
    this->clear();
    auto opt = open_map(path, O_RDONLY, PROT_READ);
    if (!opt)
        return false;
    _fd = opt->fd;
    _addr = opt->addr;
    _size = opt->size;
    _read_only = true;
    _path = path;
    return true;
}

bool MappedFile::open_read_write(std::string_view path)
{
    this->clear();
    auto opt = open_map(path, O_RDWR, PROT_READ | PROT_WRITE);
    if (!opt)
        return false;
    _fd = opt->fd;
    _addr = opt->addr;
    _size = opt->size;
    _read_only = false;
    _path = path;
    return true;
}

bool MappedFile::clear()
{
    bool ret = true;
    if (_addr != nullptr && _addr != MAP_FAILED)
    {
        if (munmap(_addr, _size) == -1)
        {
            SIHD_LOG(error, "MappedFile: munmap: {}", os::last_error_str());
            ret = false;
        }
    }
    if (_fd >= 0)
        ::close(_fd);
    _fd = -1;
    _size = 0;
    _addr = nullptr;
    _read_only = false;
    _path.clear();
    return ret;
}

bool MappedFile::sync(bool async)
{
    if (_addr == nullptr)
        return false;
    if (msync(_addr, _size, async ? MS_ASYNC : MS_SYNC) == -1)
    {
        SIHD_LOG(error, "MappedFile: msync: {}", os::last_error_str());
        return false;
    }
    return true;
}

#else

// no POSIX shared mapping (android bionic, emscripten)

bool MappedFile::create(std::string_view, size_t, mode_t)
{
    SIHD_LOG(error, "MappedFile: not supported on this platform");
    return false;
}

bool MappedFile::open_read_only(std::string_view)
{
    SIHD_LOG(error, "MappedFile: not supported on this platform");
    return false;
}

bool MappedFile::open_read_write(std::string_view)
{
    SIHD_LOG(error, "MappedFile: not supported on this platform");
    return false;
}

bool MappedFile::clear()
{
    return true;
}

bool MappedFile::sync(bool)
{
    return false;
}

#endif

} // namespace sihd::sys
