#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

// create()/attach()/clear() live in src/linux|windows/SharedMemory.cpp

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

SharedMemory::SharedMemory(): _fd(-1), _size(0), _addr(nullptr), _created(false) {}

SharedMemory::~SharedMemory()
{
    this->clear();
}

} // namespace sihd::sys
