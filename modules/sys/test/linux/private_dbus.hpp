#ifndef __SIHD_SYS_TEST_PRIVATE_DBUS_HPP__
#define __SIHD_SYS_TEST_PRIVATE_DBUS_HPP__

#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/time.hpp>

namespace test
{

// no servicedir: nothing on this bus can activate a real service of the desktop
inline constexpr const char *private_dbus_config = R"(<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:path={}/bus</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
)";

class PrivateDBus
{
    public:
        ~PrivateDBus() { _daemon.terminate(); }

        static bool available() { return sihd::sys::os::exists_in_path("dbus-daemon"); }

        bool start()
        {
            const std::string config_path = sihd::sys::fs::combine(_tmp_dir.path(), "bus.conf");
            if (!sihd::sys::fs::write(config_path, fmt::format(private_dbus_config, _tmp_dir.path())))
                return false;
            _daemon.add_argv(std::vector<std::string> {"dbus-daemon",
                                                       "--config-file=" + config_path,
                                                       "--nofork",
                                                       "--print-address=1"});
            _daemon.stdout_to(_address);
            if (!_daemon.execute() || !_daemon.read_pipes(5000) || !_address.ends_with('\n'))
                return false;
            _address.pop_back();
            return true;
        }

        const std::string & address() const { return _address; }

    private:
        sihd::sys::TmpDir _tmp_dir;
        sihd::sys::Process _daemon;
        std::string _address;
};

// skips what the bus sends unasked (NameAcquired...): nullopt on timeout
inline std::optional<sihd::sys::linux_::dbus::Message> receive_member(sihd::sys::linux_::dbus::Connection & connection,
                                                                      const std::string & member)
{
    while (true)
    {
        auto received = sihd::sys::linux_::dbus::Message::receive(connection,
                                                                  sihd::util::Duration(sihd::util::time::sec(5)));
        if (!received.has_value() || !received->has_value())
            return std::nullopt;
        if ((*received)->member() == member)
            return std::move(**received);
    }
}

} // namespace test

#endif
