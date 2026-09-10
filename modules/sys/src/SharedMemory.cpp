#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

// create()/attach()/clear() live in src/linux|windows/SharedMemory.cpp

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

SharedMemory::SharedMemory(): _fd(-1), _size(0), _addr(nullptr), _created(false) {}

SharedMemory::SharedMemory(SharedMemory && other):
    _fd(other._fd),
    _size(other._size),
    _addr(other._addr),
    _created(other._created),
    _id(std::move(other._id))
{
    other._fd = -1;
    other._size = 0;
    other._addr = nullptr;
    other._created = false;
    other._id.clear();
}

SharedMemory & SharedMemory::operator=(SharedMemory && other)
{
    if (this != &other)
    {
        this->clear();
        _fd = other._fd;
        _size = other._size;
        _addr = other._addr;
        _created = other._created;
        _id = std::move(other._id);
        other._fd = -1;
        other._size = 0;
        other._addr = nullptr;
        other._created = false;
        other._id.clear();
    }
    return *this;
}

SharedMemory::~SharedMemory()
{
    this->clear();
}

} // namespace sihd::sys
