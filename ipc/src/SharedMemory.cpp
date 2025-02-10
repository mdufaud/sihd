#include <sihd/ipc/SharedMemory.hpp>
#include <sihd/util/Logger.hpp>

// ftruncate
#include <unistd.h>
// strerror
#include <string.h>

#if !defined(__SIHD_WINDOWS__)
// shm_open shm_unlink
# include <sys/mman.h>
#endif

// For O_* constants
#include <fcntl.h>

namespace sihd::ipc
{

SIHD_NEW_LOGGER("sihd::ipc");

namespace
{

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__)

struct Shm
{
        int fd;
        void *addr;
};

bool __open(int *fd, std::string_view id, mode_t mode, int shm_flags)
{
    *fd = shm_open(id.data(), shm_flags, mode);
    if (*fd == -1)
        SIHD_LOG(error, "SharedMemory: shm_open: {}", strerror(errno));
    return *fd >= 0;
}

bool __mmap(void **addr, size_t size, int fd, int mmap_flags)
{
    *addr = mmap(nullptr, size, mmap_flags, MAP_SHARED, fd, 0);
    if (*addr == MAP_FAILED)
        SIHD_LOG(error, "SharedMemory: mmap: {}", strerror(errno));
    return *addr != MAP_FAILED;
}

std::optional<Shm> create_shm(std::string_view id, size_t size, mode_t mode, int shm_flags, int mmap_flags)
{
    int fd;
    void *addr;

    if (!__open(&fd, id, mode, shm_flags))
        return std::nullopt;

    if (ftruncate(fd, size) == -1)
    {
        SIHD_LOG(error, "SharedMemory: ftruncate: {}", strerror(errno));
        return std::nullopt;
    }

    if (!__mmap(&addr, size, fd, mmap_flags))
        return std::nullopt;

    return Shm {fd, addr};
}

std::optional<Shm> shm_attach(std::string_view id, size_t size, mode_t mode, int shm_flags, int mmap_flags)
{
    int fd;
    void *addr;

    if (!__open(&fd, id, mode, shm_flags))
        return std::nullopt;

    if (!__mmap(&addr, size, fd, mmap_flags))
        return std::nullopt;

    return Shm {fd, addr};
}

#endif

} // namespace

SharedMemory::SharedMemory(): _fd(-1), _size(0), _addr(nullptr), _created(false) {}

SharedMemory::~SharedMemory()
{
    this->clear();
}

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__)

bool SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    this->clear();
    auto opt = create_shm(id, size, mode, O_RDWR | O_CREAT, PROT_READ | PROT_WRITE);
    if (opt)
    {
        _fd = opt->fd;
        _addr = opt->addr;

        _created = true;
        _id = id;
        _size = size;
    }
    return opt.has_value();
}

bool SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    this->clear();

    auto opt = shm_attach(id, size, mode, O_RDWR, PROT_READ | PROT_WRITE);
    if (opt)
    {
        _fd = opt->fd;
        _addr = opt->addr;

        _created = true;
        _id = id;
        _size = size;
    }
    return opt.has_value();
}

bool SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    this->clear();

    auto opt = shm_attach(id, size, mode, O_RDONLY, PROT_READ);
    if (opt)
    {
        _fd = opt->fd;
        _addr = opt->addr;

        _created = true;
        _id = id;
        _size = size;
    }
    return opt.has_value();
}

bool SharedMemory::clear()
{
    bool ret = true;
    if (_addr != nullptr && _addr != MAP_FAILED)
    {
        if (_created && munmap(_addr, _size) == -1)
        {
            SIHD_LOG(error, "SharedMemory: munmap: {}", strerror(errno));
            ret = false;
        }
    }
    if (_fd >= 0)
    {
        if (_created && shm_unlink(_id.c_str()) == -1)
        {
            SIHD_LOG(error, "SharedMemory: shm_unlink: {}", strerror(errno));
            ret = false;
        }
        _fd = -1;
        _id.clear();
    }
    _size = 0;
    _addr = nullptr;
    _created = false;
    return ret;
}

#else

# pragma message("TODO")

bool SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    (void)id;
    (void)size;
    (void)mode;
    return false;
}

bool SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    (void)id;
    (void)size;
    (void)mode;
    return false;
}

bool SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    (void)id;
    (void)size;
    (void)mode;
    return false;
}

bool SharedMemory::clear()
{
    return false;
}

#endif

} // namespace sihd::ipc
