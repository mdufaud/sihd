#ifndef __SIHD_SYS_POWERINHIBITOR_HPP__
#define __SIHD_SYS_POWERINHIBITOR_HPP__

#include <memory>
#include <string>

#include <sihd/sys/desktop.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys
{

class PowerInhibitor
{
    public:
        static constexpr bool supported = desktop::supported;

        PowerInhibitor();
        PowerInhibitor(PowerInhibitor && other);
        PowerInhibitor & operator=(PowerInhibitor && other);
        ~PowerInhibitor();

        PowerInhibitor(const PowerInhibitor &) = delete;
        PowerInhibitor & operator=(const PowerInhibitor &) = delete;

        void set_app_name(const std::string & name);

        sihd::util::Result<void> inhibit_sleep(sihd::util::ZStringView reason);
        sihd::util::Result<void> inhibit_display(sihd::util::ZStringView reason);
        // ends both, even when one of them reports an error
        sihd::util::Result<void> release();

        bool is_sleep_inhibited() const;
        bool is_display_inhibited() const;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
