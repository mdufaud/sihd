#ifndef __SIHD_SYS_MEMLOCK_HPP__
#define __SIHD_SYS_MEMLOCK_HPP__

#include <cstddef>

#include <sihd/sys/os.hpp>

namespace sihd::sys
{

class MemLock
{
    public:
        MemLock(const void *addr, size_t len);
        MemLock(MemLock && other);
        MemLock & operator=(MemLock && other);
        ~MemLock();

        MemLock(const MemLock &) = delete;
        MemLock & operator=(const MemLock &) = delete;

        bool locked() const { return _locked; }

        void unlock();

    protected:

    private:
        const void *_addr;
        size_t _len;
        bool _locked;
};

} // namespace sihd::sys

#endif
