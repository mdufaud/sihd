#include <sihd/util/SharedMemory.hpp>
#include <sihd/util/Logger.hpp>

// ftruncate
#include <unistd.h>
// strerror
#include <string.h>

#if !defined(__SIHD_WINDOWS__)
// shm_open shm_unlink
# include <sys/mman.h>
// For mode constants
# include <sys/stat.h>
#endif

// For O_* constants
#include <fcntl.h>

namespace sihd::util
{

SIHD_LOGGER;

SharedMemory::SharedMemory(): _fd(-1), _size(0), _addr(nullptr), _created(false)
{
}

SharedMemory::~SharedMemory()
{
    this->clear();
}

#if !defined(__SIHD_WINDOWS__)

bool    SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    return this->_create(id, size, mode, O_RDWR | O_CREAT, PROT_READ | PROT_WRITE);
}

bool    SharedMemory::_create(std::string_view id, size_t size, mode_t mode, int shm_flags, int mmap_flags)
{
    _fd = shm_open(id.data(), shm_flags, mode);
    if (_fd == -1)
    {
        SIHD_LOG(error, "SharedMemory: shm_open: " << strerror(errno));
        return false;
    }
    _created = true;
    _id = id;
    _size = size;
    if (ftruncate(_fd, size) == -1)
    {
        this->clear();
        SIHD_LOG(error, "SharedMemory: ftruncate: " << strerror(errno));
        return false;
    }
    _addr = mmap(nullptr, size, mmap_flags, MAP_SHARED, _fd, 0);
    if (_addr == MAP_FAILED)
    {
        this->clear();
        SIHD_LOG(error, "SharedMemory: mmap: " << strerror(errno));
        return false;
    }
    return true;
}

bool    SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    return this->_attach(id, size, mode, O_RDWR, PROT_READ | PROT_WRITE);
}

bool    SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    return this->_attach(id, size, mode, O_RDONLY, PROT_READ);
}

bool    SharedMemory::_attach(std::string_view id, size_t size, mode_t mode, int shm_flags, int mmap_flags)
{
    _fd = shm_open(id.data(), shm_flags, mode);
    if (_fd == -1)
    {
        SIHD_LOG(error, "SharedMemory: shm_open: " << strerror(errno));
        return false;
    }
    _created = false;
    _id = id;
    _size = size;
    _addr = mmap(nullptr, size, mmap_flags, MAP_SHARED, _fd, 0);
    if (_addr == MAP_FAILED)
    {
        this->clear();
        SIHD_LOG(error, "SharedMemory: mmap: " << strerror(errno));
        return false;
    }
    return true;
}

bool    SharedMemory::clear()
{
    bool ret = true;
    if (_addr != nullptr && _addr != MAP_FAILED)
    {
        if (_created && munmap(_addr, _size) == -1)
        {
            SIHD_LOG(error, "SharedMemory: munmap: " << strerror(errno));
            ret = false;
        }
    }
    if (_fd >= 0)
    {
        if (_created && shm_unlink(_id.c_str()) == -1)
        {
            SIHD_LOG(error, "SharedMemory: shm_unlink: " << strerror(errno));
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

bool    SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    (void)id; (void)size; (void)mode;
    return false;
}

bool    SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    (void)id; (void)size; (void)mode;
    return false;
}

bool    SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    (void)id; (void)size; (void)mode;
    return false;
}

bool    SharedMemory::clear()
{
    return false;
}

#endif

}