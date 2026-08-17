#ifndef __SIHD_SYS_CAPABILITY_SET_HPP__
#define __SIHD_SYS_CAPABILITY_SET_HPP__

#include <memory>
#include <vector>

#include <sihd/sys/cap.hpp>

namespace sihd::sys
{

// Snapshot of the caller's privileges. linux: libcap permitted/effective sets, held PER THREAD.
// windows: PROCESS token privileges. refresh() loads, raise()/drop() mutate, apply() commits;
// is_enabled()/permitted()/enabled() read the snapshot. cap::has() reads the live state.
class CapabilitySet
{
    public:
        CapabilitySet();
        ~CapabilitySet();

        CapabilitySet(const CapabilitySet &) = delete;
        CapabilitySet & operator=(const CapabilitySet &) = delete;
        CapabilitySet(CapabilitySet &&) = delete;
        CapabilitySet & operator=(CapabilitySet &&) = delete;

        // reloads the snapshot, discarding uncommitted changes
        bool refresh();

        // === query (never requires privileges) ===

        // state in the SNAPSHOT: reflects raise()/drop() that apply() has not committed yet
        bool is_enabled(Cap cap) const;
        // capability can be raised into the effective set
        bool permitted(Cap cap) const;
        // every capability is_enabled() in the snapshot
        std::vector<Cap> enabled() const;

        // === snapshot mutation, committed by apply() ===

        // fails if the capability is not permitted
        bool raise(Cap cap);
        // dropping a capability that is not held is a successful no-op
        bool drop(Cap cap);
        void drop_all();

        bool apply();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
