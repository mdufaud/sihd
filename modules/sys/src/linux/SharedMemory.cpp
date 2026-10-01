#include <fcntl.h>    // O_*
#include <string.h>   // strerror
#include <sys/mman.h> // shm_open, mmap, munmap
#include <unistd.h>   // ftruncate, close

#include <sihd/sys/SharedMemory.hpp>
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

struct Shm
{
        int fd;
        void *addr;
};

std::expected<Shm, Error> __open(std::string_view id, mode_t mode, int shm_flags)
{
    int fd = shm_open(id.data(), shm_flags, mode);
    if (fd == -1)
        return std::unexpected(Error::from_errno("shm_open"));
    return Shm {fd, nullptr};
}

std::expected<Shm, Error> __mmap(Shm shm, size_t size, int mmap_flags)
{
    void *addr = mmap(nullptr, size, mmap_flags, MAP_SHARED, shm.fd, 0);
    if (addr == MAP_FAILED)
        return std::unexpected(Error::from_errno("mmap"));
    shm.addr = addr;
    return shm;
}

std::expected<Shm, Error> create_shm(std::string_view id, size_t size, mode_t mode, int shm_flags, int mmap_flags)
{
    auto shm = __open(id, mode, shm_flags);
    if (!shm)
        return shm;

    // ftruncate
    if (ftruncate(shm->fd, size) == -1)
    {
        auto error = Error::from_errno("ftruncate");
        close(shm->fd);
        return std::unexpected(std::move(error));
    }

    auto mapped = __mmap(*shm, size, mmap_flags);
    if (!mapped)
        close(shm->fd);
    return mapped;
}

std::expected<Shm, Error> shm_attach(std::string_view id, size_t size, mode_t mode, int shm_flags, int mmap_flags)
{
    auto shm = __open(id, mode, shm_flags);
    if (!shm)
        return shm;

    auto mapped = __mmap(*shm, size, mmap_flags);
    if (!mapped)
        close(shm->fd);
    return mapped;
}

} // namespace

std::expected<void, Error> SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;
    auto shm = create_shm(id, size, mode, O_RDWR | O_CREAT | O_EXCL, PROT_READ | PROT_WRITE);
    SIHD_UNEXPECTED_RETURN(shm);

    _fd = shm->fd;
    _addr = shm->addr;

    _created = true;
    _id = id;
    _size = size;
    return {};
}

std::expected<void, Error> SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    auto shm = shm_attach(id, size, mode, O_RDWR, PROT_READ | PROT_WRITE);
    SIHD_UNEXPECTED_RETURN(shm);

    _fd = shm->fd;
    _addr = shm->addr;

    _created = false;
    _id = id;
    _size = size;
    return {};
}

std::expected<void, Error> SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    auto shm = shm_attach(id, size, mode, O_RDONLY, PROT_READ);
    SIHD_UNEXPECTED_RETURN(shm);

    _fd = shm->fd;
    _addr = shm->addr;

    _created = false;
    _id = id;
    _size = size;
    return {};
}

std::expected<void, Error> SharedMemory::clear()
{
    bool ret = true;
    if (_addr != nullptr && _addr != MAP_FAILED)
    {
        if (munmap(_addr, _size) == -1)
            ret = false;
    }
    if (_fd >= 0)
    {
        if (_created && shm_unlink(_id.c_str()) == -1)
            ret = false;
        close(_fd);
        _fd = -1;
        _id.clear();
    }
    _size = 0;
    _addr = nullptr;
    _created = false;
    if (!ret)
        return std::unexpected(Error(io_error, "could not unmap shared memory"));
    return {};
}

#else

// no POSIX shared memory (android bionic, emscripten)

std::expected<void, Error> SharedMemory::create(std::string_view, size_t, mode_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> SharedMemory::attach(std::string_view, size_t, mode_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> SharedMemory::attach_read_only(std::string_view, size_t, mode_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> SharedMemory::clear()
{
    return {};
}

#endif

} // namespace sihd::sys
