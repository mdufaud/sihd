#ifndef __SIHD_SYS_CAPABILITY_SET_HPP__
#define __SIHD_SYS_CAPABILITY_SET_HPP__

#include <memory>
#include <vector>

#include <sihd/sys/cap.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys
{

// snapshot of the caller's privileges (linux: libcap sets per thread, windows: token); cap::has() reads live
class CapabilitySet
{
    public:
        CapabilitySet();
        ~CapabilitySet();

        CapabilitySet(const CapabilitySet &) = delete;
        CapabilitySet & operator=(const CapabilitySet &) = delete;
        CapabilitySet(CapabilitySet &&) = delete;
        CapabilitySet & operator=(CapabilitySet &&) = delete;

        sihd::util::Result<void> refresh();

        // state in the SNAPSHOT: reflects raise()/drop() that apply() has not committed yet
        bool is_enabled(Cap cap) const;
        bool permitted(Cap cap) const;
        std::vector<Cap> enabled() const;

        bool raise(Cap cap);
        // dropping a capability that is not held is a successful no-op
        bool drop(Cap cap);
        void drop_all();

        sihd::util::Result<void> apply();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
