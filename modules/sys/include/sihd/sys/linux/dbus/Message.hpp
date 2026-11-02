#ifndef __SIHD_SYS_LINUX_DBUS_MESSAGE_HPP__
#define __SIHD_SYS_LINUX_DBUS_MESSAGE_HPP__

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/linux/dbus/Connection.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::linux_::dbus
{

template <typename T>
concept BasicType = std::same_as<T, bool> || std::same_as<T, uint8_t> || std::same_as<T, int16_t>
                    || std::same_as<T, uint16_t> || std::same_as<T, int32_t> || std::same_as<T, uint32_t>
                    || std::same_as<T, int64_t> || std::same_as<T, uint64_t> || std::same_as<T, double>
                    || std::same_as<T, std::string>;

template <typename T>
concept FixedType = BasicType<T> && !std::same_as<T, std::string>;

class Connection::Message
{
    public:
        enum class Type
        {
            invalid,
            method_call,
            method_return,
            error,
            signal
        };

        class Iter;
        class Appender;

        Message(Message && other);
        Message & operator=(Message && other);
        ~Message();

        Message(const Message &) = delete;
        Message & operator=(const Message &) = delete;

        static sihd::util::Result<Message> method_call(sihd::util::ZStringView destination,
                                                       sihd::util::ZStringView path,
                                                       sihd::util::ZStringView interface,
                                                       sihd::util::ZStringView member);
        static sihd::util::Result<Message>
            signal(sihd::util::ZStringView path, sihd::util::ZStringView interface, sihd::util::ZStringView member);
        // a Get reply holds one variant, a GetAll reply one a{sv}
        static sihd::util::Result<Message> properties_get(sihd::util::ZStringView destination,
                                                          sihd::util::ZStringView path,
                                                          sihd::util::ZStringView interface,
                                                          sihd::util::ZStringView name);
        static sihd::util::Result<Message> properties_get_all(sihd::util::ZStringView destination,
                                                              sihd::util::ZStringView path,
                                                              sihd::util::ZStringView interface);
        sihd::util::Result<Message> method_return() const;
        sihd::util::Result<Message> error_return(sihd::util::ZStringView name, sihd::util::ZStringView text) const;

        // empty when the message has none
        std::string path() const;
        std::string interface() const;
        std::string member() const;
        std::string sender() const;
        std::string destination() const;
        std::string error_name() const;
        std::string signature() const;
        Type type() const;
        bool expects_reply() const;
        // the receiver of this call must not answer it, a sent or received message stays as it is
        sihd::util::Result<void> set_no_reply();
        // "<name>: <message>", coded like Call::fail encodes (docs/modules/sys.md), an unknown name is io_error
        sihd::util::Error to_error() const;
        // nullopt unless a PropertiesChanged signal of that interface carries that property
        sihd::util::Result<std::optional<Iter>> changed_property(std::string_view interface,
                                                                 std::string_view name) const;

        template <FixedType T>
        sihd::util::Result<void> add(const T & value);
        sihd::util::Result<void> add(sihd::util::ZStringView value);
        sihd::util::Result<void> add_object_path(sihd::util::ZStringView path);
        sihd::util::Result<void> add_signature(sihd::util::ZStringView signature);
        sihd::util::Result<void> add_unix_fd(int fd);

        // an error reply fails like its to_error()
        sihd::util::Result<Message> call(Connection & connection, sihd::util::Duration timeout) const;
        // every request leaves before the first reply is awaited: the replies come in request order
        static sihd::util::Result<std::vector<Message>>
            call_all(Connection & connection, const std::vector<Message> & requests, sihd::util::Duration timeout);
        sihd::util::Result<void> send(Connection & connection) const;
        // waits for an incoming message: nullopt once the timeout elapsed or connection.interrupt() was called
        static sihd::util::Result<std::optional<Message>> receive(Connection & connection,
                                                                  sihd::util::Duration timeout);
        // the first incoming message accepted, else a timeout error or, on connection.interrupt(), an interrupted one
        static sihd::util::Result<Message>
            receive_until(Connection & connection,
                          sihd::util::Duration timeout,
                          const std::function<sihd::util::Result<bool>(const Message &)> & accepted);

    private:
        struct Impl;

        explicit Message(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> _impl;
};

using Message = Connection::Message;

} // namespace sihd::sys::linux_::dbus

#endif
