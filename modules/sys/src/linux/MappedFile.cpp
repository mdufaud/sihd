#include <fcntl.h>    // O_*
#include <sys/mman.h> // mmap, munmap, msync
#include <sys/stat.h>
#include <unistd.h> // close, ftruncate

#include <sihd/sys/MappedFile.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
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

std::expected<Mapping, Error> map_fd(int fd, size_t size, int prot, std::string_view path)
{
    void *addr = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        return std::unexpected(Error::from_errno("could not mmap '{}' ({} bytes)", path, size));
    return Mapping {fd, addr, size};
}

std::expected<Mapping, Error> open_map(std::string_view path, int open_flags, int prot)
{
    int fd = ::open(path.data(), open_flags);
    if (fd < 0)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    struct stat s;
    if (fstat(fd, &s) == -1)
    {
        auto error = Error::from_errno("could not stat '{}'", path);
        ::close(fd);
        return std::unexpected(std::move(error));
    }
    if (s.st_size <= 0)
    {
        ::close(fd);
        return std::unexpected(Error(invalid_argument, "cannot map an empty file '{}'", path));
    }
    return map_fd(fd, (size_t)s.st_size, prot, path);
}

} // namespace

std::expected<void, Error> MappedFile::create(std::string_view path, size_t size, mode_t mode)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;
    if (size == 0)
        return std::unexpected(Error(invalid_argument, "cannot map an empty size"));
    int fd = ::open(path.data(), O_RDWR | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    if (ftruncate(fd, size) == -1)
    {
        auto error = Error::from_errno("could not truncate '{}'", path);
        ::close(fd);
        return std::unexpected(std::move(error));
    }
    auto mapped = map_fd(fd, size, PROT_READ | PROT_WRITE, path);
    if (!mapped)
    {
        ::close(fd);
        SIHD_UNEXPECTED_RETURN(mapped);
    }
    _fd = mapped->fd;
    _addr = mapped->addr;
    _size = mapped->size;
    _read_only = false;
    _path = path;
    return {};
}

std::expected<void, Error> MappedFile::open_read_only(std::string_view path)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;
    auto mapped = open_map(path, O_RDONLY, PROT_READ);
    SIHD_UNEXPECTED_RETURN(mapped);
    _fd = mapped->fd;
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
    auto mapped = open_map(path, O_RDWR, PROT_READ | PROT_WRITE);
    SIHD_UNEXPECTED_RETURN(mapped);
    _fd = mapped->fd;
    _addr = mapped->addr;
    _size = mapped->size;
    _read_only = false;
    _path = path;
    return {};
}

std::expected<void, Error> MappedFile::clear()
{
    bool ret = true;
    if (_addr != nullptr && _addr != MAP_FAILED)
    {
        if (munmap(_addr, _size) == -1)
            ret = false;
    }
    if (_fd >= 0)
        ::close(_fd);
    _fd = -1;
    _size = 0;
    _addr = nullptr;
    _read_only = false;
    _path.clear();
    if (!ret)
        return std::unexpected(Error::from_errno("could not unmap"));
    return {};
}

std::expected<void, Error> MappedFile::sync(bool async)
{
    if (_addr == nullptr)
        return std::unexpected(Error(not_initialized, "no mapping"));
    if (msync(_addr, _size, async ? MS_ASYNC : MS_SYNC) == -1)
        return std::unexpected(Error::from_errno("could not sync"));
    return {};
}

#else

// no POSIX shared mapping (android bionic, emscripten)

std::expected<void, Error> MappedFile::create(std::string_view, size_t, mode_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> MappedFile::open_read_only(std::string_view)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> MappedFile::open_read_write(std::string_view)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> MappedFile::clear()
{
    return {};
}

std::expected<void, Error> MappedFile::sync(bool)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

#endif

} // namespace sihd::sys
