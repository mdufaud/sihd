#include <sihd/sys/MemLock.hpp>

namespace sihd::sys
{

MemLock::MemLock(const void *addr, size_t len): _addr(addr), _len(len), _locked(os::mem_lock(addr, len).has_value()) {}

MemLock::MemLock(MemLock && other): _addr(other._addr), _len(other._len), _locked(other._locked)
{
    other._addr = nullptr;
    other._len = 0;
    other._locked = false;
}

MemLock & MemLock::operator=(MemLock && other)
{
    if (this != &other)
    {
        this->unlock();
        _addr = other._addr;
        _len = other._len;
        _locked = other._locked;
        other._addr = nullptr;
        other._len = 0;
        other._locked = false;
    }
    return *this;
}

MemLock::~MemLock()
{
    this->unlock();
}

void MemLock::unlock()
{
    if (_locked)
    {
        (void)os::mem_unlock(_addr, _len);
        _locked = false;
    }
}

} // namespace sihd::sys
