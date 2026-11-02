#ifndef __SIHD_SYS_LINUX_DBUS_ITER_HPP__
#define __SIHD_SYS_LINUX_DBUS_ITER_HPP__

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <sihd/sys/linux/dbus/Message.hpp>
#include <sihd/sys/linux/dbus/UnixFd.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::linux_::dbus
{

// forward-only reader over a message body or container, valid while its message lives
class Connection::Message::Iter
{
    public:
        explicit Iter(const Message & message);
        Iter(Iter && other);
        Iter & operator=(Iter && other);
        ~Iter();

        Iter(const Iter &) = delete;
        Iter & operator=(const Iter &) = delete;

        // dbus type code of the current element, '\0' past the end
        char type() const;
        std::string signature() const;
        bool next();

        // std::string reads strings, object paths and signatures
        template <BasicType T>
        sihd::util::Result<T> get() const;
        template <BasicType T>
        sihd::util::Result<std::vector<T>> get_array() const;
        sihd::util::Result<UnixFd> get_unix_fd() const;
        // the callback never sees a variant: its value comes unwrapped
        template <BasicType Key>
        sihd::util::Result<void>
            for_each_entry(const std::function<sihd::util::Result<void>(const Key &, Iter &)> & callback) const;

        sihd::util::Result<Iter> recurse() const;

    private:
        struct Impl;

        explicit Iter(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> _impl;
};

using Iter = Connection::Message::Iter;

} // namespace sihd::sys::linux_::dbus

#endif
