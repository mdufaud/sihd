#ifndef __SIHD_SYS_LINUX_DBUS_APPENDER_HPP__
#define __SIHD_SYS_LINUX_DBUS_APPENDER_HPP__

#include <initializer_list>
#include <memory>
#include <string>

#include <sihd/sys/linux/dbus/Message.hpp>
#include <sihd/sys/linux/dbus/Value.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::linux_::dbus
{

// valid while its message lives, no Message::add nor builder meanwhile
class Connection::Message::Appender
{
    public:
        explicit Appender(Message & message);
        Appender(Appender && other);
        Appender & operator=(Appender && other);
        ~Appender();

        Appender(const Appender &) = delete;
        Appender & operator=(const Appender &) = delete;

        // a value whose container content fails leaves the message abandoned
        sihd::util::Result<void> add(const Value & value);
        template <typename... T>
            requires(sizeof...(T) > 1)
        sihd::util::Result<void> add(const T &...values)
        {
            for (const Value & value : {Value(values)...})
            {
                auto added = this->add(value);
                if (!added.has_value())
                    return added;
            }
            return {};
        }
        // the message keeps its own duplicate, fd stays the caller's
        sihd::util::Result<void> add_unix_fd(int fd);

        sihd::util::Result<Appender> open_array(const std::string & element_signature);
        sihd::util::Result<Appender> open_variant(const std::string & value_signature);
        sihd::util::Result<Appender> open_struct();
        sihd::util::Result<Appender> open_dict_entry();
        // a container dropped without close() is abandoned and so is its message
        sihd::util::Result<void> close(Appender && container);

    private:
        struct Impl;
        struct Writer;

        explicit Appender(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> _impl;
};

using Appender = Connection::Message::Appender;

} // namespace sihd::sys::linux_::dbus

#endif
