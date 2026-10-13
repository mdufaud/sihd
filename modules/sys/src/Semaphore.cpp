#include <sihd/sys/Semaphore.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

// create()/attach()/clear() and the wait/post family live in src/linux|windows/Semaphore.cpp

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

Semaphore::Semaphore(): _sem(nullptr), _created(false) {}

Semaphore::Semaphore(Semaphore && other): _sem(other._sem), _created(other._created), _name(std::move(other._name))
{
    other._sem = nullptr;
    other._created = false;
    other._name.clear();
}

Semaphore & Semaphore::operator=(Semaphore && other)
{
    if (this != &other)
    {
        (void)this->clear();
        _sem = other._sem;
        _created = other._created;
        _name = std::move(other._name);
        other._sem = nullptr;
        other._created = false;
        other._name.clear();
    }
    return *this;
}

Semaphore::~Semaphore()
{
    (void)this->clear();
}

} // namespace sihd::sys
