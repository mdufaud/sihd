#ifndef __SIHD_SYS_LINUX_DBUS_APPENDER_HPP__
#define __SIHD_SYS_LINUX_DBUS_APPENDER_HPP__

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <sihd/sys/linux/dbus/Message.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

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

        template <FixedType T>
        sihd::util::Result<void> add(const T & value);
        sihd::util::Result<void> add(sihd::util::ZStringView value);
        sihd::util::Result<void> add_object_path(sihd::util::ZStringView path);
        sihd::util::Result<void> add_signature(sihd::util::ZStringView signature);
        // the message keeps its own duplicate, fd stays the caller's
        sihd::util::Result<void> add_unix_fd(int fd);
        template <BasicType T>
        sihd::util::Result<void> add_array(const std::vector<T> & values);
        // a{Kv} holding an ay in every variant
        template <BasicType Key>
        sihd::util::Result<void> add_byte_map(const std::map<Key, std::vector<uint8_t>> & map);
        // the a{sv} of no option
        sihd::util::Result<void> add_empty_vardict();
        // one {sv} entry of an a{sv} array: fill writes the value, of that signature
        sihd::util::Result<void> add_entry(sihd::util::ZStringView key,
                                           const std::string & signature,
                                           const std::function<sihd::util::Result<void>(Appender &)> & fill);

        sihd::util::Result<Appender> open_array(const std::string & element_signature);
        sihd::util::Result<Appender> open_variant(const std::string & value_signature);
        sihd::util::Result<Appender> open_struct();
        sihd::util::Result<Appender> open_dict_entry();
        // a container dropped without close() is abandoned and so is its message
        sihd::util::Result<void> close(Appender && container);

    private:
        struct Impl;

        explicit Appender(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> _impl;
};

using Appender = Connection::Message::Appender;

} // namespace sihd::sys::linux_::dbus

#endif
