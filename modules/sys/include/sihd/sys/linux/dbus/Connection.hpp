#ifndef __SIHD_SYS_LINUX_DBUS_CONNECTION_HPP__
#define __SIHD_SYS_LINUX_DBUS_CONNECTION_HPP__

#include <memory>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::dbus
{

enum class Bus
{
    session,
    system
};

struct NameOptions
{
        // another connection asking with replace_existing takes the name, this one gets a NameLost signal
        bool allow_replacement = false;
        bool replace_existing = false;
        // waits behind the owner instead of failing: a NameAcquired signal comes once it is primary
        bool queue = false;
};

enum class NameRequest
{
    primary_owner,
    queued
};

// never trust an invalid user or a zero pid: the bus did not tell them
struct Credentials
{
        sihd::sys::user::UserId user;
        pid_t pid = 0;
        std::vector<sihd::sys::user::GroupId> groups;
};

// a thread calling while another one receives needs its own connection: receive() can steal the reply call() awaits
class Connection
{
    public:
        class Message;

        static constexpr bool supported = !sihd::util::build::is_android && !sihd::util::build::is_emscripten;

        Connection();
        Connection(Connection && other);
        Connection & operator=(Connection && other);
        ~Connection();

        Connection(const Connection &) = delete;
        Connection & operator=(const Connection &) = delete;

        // replaces an open connection on success only
        sihd::util::Result<void> open(Bus bus);
        sihd::util::Result<void> open(sihd::util::ZStringView address);
        bool is_open() const;
        void close();

        // dbus match rule, e.g. "type='signal',sender='org.bluez',member='InterfacesAdded'"
        sihd::util::Result<void> add_match(sihd::util::ZStringView rule);
        sihd::util::Result<void> remove_match(sihd::util::ZStringView rule);
        // owns a well-known bus name, already_exists when another connection holds it and no queue was asked
        sihd::util::Result<NameRequest> request_name(sihd::util::ZStringView name, const NameOptions & options = {});
        // not_found when nobody owns the name, invalid_argument when another connection does
        sihd::util::Result<void> release_name(sihd::util::ZStringView name);
        // asks the bus driver: never starts an activatable service
        sihd::util::Result<bool> name_has_owner(sihd::util::ZStringView name);
        sihd::util::Result<Credentials> credentials(sihd::util::ZStringView bus_name, sihd::util::Duration timeout);
        // a call sent from here on fails instead of starting an unowned activatable service
        void disable_auto_start();
        // wakes a Message::receive() blocked in another thread
        sihd::util::Result<void> interrupt();

    private:
        struct Impl;

        sihd::util::Result<void> _check_name(sihd::util::ZStringView name) const;

        std::unique_ptr<Impl> _impl;
        bool _auto_start = true;
};

} // namespace sihd::sys::linux_::dbus

#endif
