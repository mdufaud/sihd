#ifndef __SIHD_SYS_LINUX_DBUS_HPP__
#define __SIHD_SYS_LINUX_DBUS_HPP__

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

template <typename T>
concept BasicType = std::same_as<T, bool> || std::same_as<T, uint8_t> || std::same_as<T, int16_t>
                    || std::same_as<T, uint16_t> || std::same_as<T, int32_t> || std::same_as<T, uint32_t>
                    || std::same_as<T, std::string>;

template <typename T>
concept FixedType = BasicType<T> && !std::same_as<T, std::string>;

class Connection
{
    public:
        class Message
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

                // forward-only reader over a message body or container, valid while its message lives
                class Iter
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
                        bool next();

                        // std::string reads strings, object paths and signatures
                        template <BasicType T>
                        sihd::util::Result<T> get() const;
                        template <BasicType T>
                        sihd::util::Result<std::vector<T>> get_array() const;
                        // the callback never sees a variant: its value comes unwrapped
                        template <BasicType Key>
                        sihd::util::Result<void> for_each_entry(
                            const std::function<sihd::util::Result<void>(const Key &, Iter &)> & callback) const;

                        sihd::util::Result<Iter> recurse() const;

                    private:
                        struct Impl;

                        explicit Iter(std::unique_ptr<Impl> impl);

                        std::unique_ptr<Impl> _impl;
                };

                // valid while its message lives, no Message::add meanwhile
                class Appender
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
                        template <BasicType T>
                        sihd::util::Result<void> add_array(const std::vector<T> & values);
                        // one {sv} entry of an a{sv} array: fill writes the value, of that signature
                        sihd::util::Result<void>
                            add_entry(sihd::util::ZStringView key,
                                      sihd::util::ZStringView signature,
                                      const std::function<sihd::util::Result<void>(Appender &)> & fill);

                        sihd::util::Result<Appender> open_array(sihd::util::ZStringView element_signature);
                        sihd::util::Result<Appender> open_variant(sihd::util::ZStringView value_signature);
                        sihd::util::Result<Appender> open_struct();
                        sihd::util::Result<Appender> open_dict_entry();
                        // a container dropped without close() is abandoned and so is its message
                        sihd::util::Result<void> close(Appender && container);

                    private:
                        struct Impl;

                        explicit Appender(std::unique_ptr<Impl> impl);

                        std::unique_ptr<Impl> _impl;
                };

                Message(Message && other);
                Message & operator=(Message && other);
                ~Message();

                Message(const Message &) = delete;
                Message & operator=(const Message &) = delete;

                static sihd::util::Result<Message> method_call(sihd::util::ZStringView destination,
                                                               sihd::util::ZStringView path,
                                                               sihd::util::ZStringView interface,
                                                               sihd::util::ZStringView member);
                static sihd::util::Result<Message> signal(sihd::util::ZStringView path,
                                                          sihd::util::ZStringView interface,
                                                          sihd::util::ZStringView member);
                // a Get reply holds one variant, a GetAll reply one a{sv}
                static sihd::util::Result<Message> properties_get(sihd::util::ZStringView destination,
                                                                  sihd::util::ZStringView path,
                                                                  sihd::util::ZStringView interface,
                                                                  sihd::util::ZStringView name);
                static sihd::util::Result<Message> properties_get_all(sihd::util::ZStringView destination,
                                                                      sihd::util::ZStringView path,
                                                                      sihd::util::ZStringView interface);
                sihd::util::Result<Message> method_return() const;
                sihd::util::Result<Message> error_return(sihd::util::ZStringView name,
                                                         sihd::util::ZStringView text) const;

                // empty when the message has none
                std::string path() const;
                std::string interface() const;
                std::string member() const;
                std::string sender() const;
                std::string error_name() const;
                Type type() const;
                // "<name>: <message>": AccessDenied is permission_denied, NoReply timeout, any other io_error
                sihd::util::Error to_error() const;
                // nullopt unless a PropertiesChanged signal of that interface carries that property
                sihd::util::Result<std::optional<Iter>> changed_property(std::string_view interface,
                                                                         std::string_view name) const;

                template <FixedType T>
                sihd::util::Result<void> add(const T & value);
                sihd::util::Result<void> add(sihd::util::ZStringView value);
                sihd::util::Result<void> add_object_path(sihd::util::ZStringView path);

                // an error reply fails like its to_error()
                sihd::util::Result<Message> call(Connection & connection, sihd::util::Duration timeout) const;
                // every request leaves before the first reply is awaited: the replies come in request order
                static sihd::util::Result<std::vector<Message>> call_all(Connection & connection,
                                                                         const std::vector<Message> & requests,
                                                                         sihd::util::Duration timeout);
                sihd::util::Result<void> send(Connection & connection) const;
                // waits for an incoming message: nullopt once the timeout elapsed or connection.interrupt() was called
                static sihd::util::Result<std::optional<Message>> receive(Connection & connection,
                                                                          sihd::util::Duration timeout);
                // the first incoming message accepted, a timeout error once the timeout elapsed
                static sihd::util::Result<Message>
                    receive_until(Connection & connection,
                                  sihd::util::Duration timeout,
                                  const std::function<sihd::util::Result<bool>(const Message &)> & accepted);

            private:
                struct Impl;

                explicit Message(std::unique_ptr<Impl> impl);

                std::unique_ptr<Impl> _impl;
        };

        static constexpr bool supported = !sihd::util::build::is_android && !sihd::util::build::is_emscripten;

        Connection();
        Connection(Connection && other);
        Connection & operator=(Connection && other);
        ~Connection();

        Connection(const Connection &) = delete;
        Connection & operator=(const Connection &) = delete;

        sihd::util::Result<void> open(Bus bus);
        sihd::util::Result<void> open(sihd::util::ZStringView address);
        bool is_open() const;
        void close();

        // dbus match rule, e.g. "type='signal',sender='org.bluez',member='InterfacesAdded'"
        sihd::util::Result<void> add_match(sihd::util::ZStringView rule);
        sihd::util::Result<void> remove_match(sihd::util::ZStringView rule);
        // owns a well-known bus name, already_exists when another connection holds it
        sihd::util::Result<void> request_name(sihd::util::ZStringView name);
        // asks the bus driver: never starts an activatable service
        sihd::util::Result<bool> name_has_owner(sihd::util::ZStringView name);
        // a call sent from here on fails instead of starting an unowned activatable service
        void disable_auto_start();
        // wakes a Message::receive() blocked in another thread
        sihd::util::Result<void> interrupt();

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
        bool _auto_start = true;
};

using Message = Connection::Message;
using Iter = Connection::Message::Iter;
using Appender = Connection::Message::Appender;

template <BasicType T>
sihd::util::Result<T> get_property(Connection & connection,
                                   sihd::util::ZStringView destination,
                                   sihd::util::ZStringView path,
                                   sihd::util::ZStringView interface,
                                   sihd::util::ZStringView name,
                                   sihd::util::Duration timeout);
template <BasicType T>
sihd::util::Result<std::vector<T>> get_array_property(Connection & connection,
                                                      sihd::util::ZStringView destination,
                                                      sihd::util::ZStringView path,
                                                      sihd::util::ZStringView interface,
                                                      sihd::util::ZStringView name,
                                                      sihd::util::Duration timeout);

std::string
    signal_rule(std::string_view sender, std::string_view path, std::string_view interface, std::string_view member);
std::string properties_changed_rule(std::string_view sender, std::string_view path, std::string_view interface);

// relies on Message::call() failing with "<error name>: <message>"
bool has_error_name(const sihd::util::Error & error, std::string_view name);

} // namespace sihd::sys::linux_::dbus

#endif
