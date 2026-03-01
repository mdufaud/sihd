#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>

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

namespace sihd::sys
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::shm");

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
        SIHD_LOG(error, "SharedMemory: shm_open: {}", os::last_error_str());
    return *fd >= 0;
}

bool __mmap(void **addr, size_t size, int fd, int mmap_flags)
{
    *addr = mmap(nullptr, size, mmap_flags, MAP_SHARED, fd, 0);
    if (*addr == MAP_FAILED)
        SIHD_LOG(error, "SharedMemory: mmap: {}", os::last_error_str());
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
        SIHD_LOG(error, "SharedMemory: ftruncate: {}", os::last_error_str());
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
            SIHD_LOG(error, "SharedMemory: munmap: {}", os::last_error_str());
            ret = false;
        }
    }
    if (_fd >= 0)
    {
        if (_created && shm_unlink(_id.c_str()) == -1)
        {
            SIHD_LOG(error, "SharedMemory: shm_unlink: {}", os::last_error_str());
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

#elif defined(__SIHD_WINDOWS__)

# include <windows.h>

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

#else

// Unsupported platform (e.g., Android, Emscripten)

bool SharedMemory::create(std::string_view id, size_t size, mode_t mode)
{
    (void)id;
    (void)size;
    (void)mode;
    SIHD_LOG(error, "SharedMemory: not supported on this platform");
    return false;
}

bool SharedMemory::attach(std::string_view id, size_t size, mode_t mode)
{
    (void)id;
    (void)size;
    (void)mode;
    SIHD_LOG(error, "SharedMemory: not supported on this platform");
    return false;
}

bool SharedMemory::attach_read_only(std::string_view id, size_t size, mode_t mode)
{
    (void)id;
    (void)size;
    (void)mode;
    SIHD_LOG(error, "SharedMemory: not supported on this platform");
    return false;
}

bool SharedMemory::clear()
{
    return true;
}

#endif

} // namespace sihd::sys
