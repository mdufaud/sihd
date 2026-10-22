#ifndef __SIHD_SYS_LINUX_DBUS_HPP__
#define __SIHD_SYS_LINUX_DBUS_HPP__

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
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

class Connection
{
    public:
        class Message
        {
            public:
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

                        sihd::util::Result<Iter> recurse() const;

                    private:
                        struct Impl;

                        explicit Iter(std::unique_ptr<Impl> impl);

                        std::unique_ptr<Impl> _impl;
                };

                Message(Message && other);
                Message & operator=(Message && other);
                ~Message();

                Message(const Message &) = delete;
                Message & operator=(const Message &) = delete;

                static sihd::util::Result<Message> method_call(const std::string & destination,
                                                               const std::string & path,
                                                               const std::string & interface,
                                                               const std::string & member);
                static sihd::util::Result<Message>
                    signal(const std::string & path, const std::string & interface, const std::string & member);

                // empty when the message has none
                std::string path() const;
                std::string interface() const;
                std::string member() const;
                std::string sender() const;

                template <BasicType T>
                sihd::util::Result<void> add(const T & value);
                sihd::util::Result<void> add(const char *value);

                // sends and waits for the reply, a dbus error reply becomes an io_error "<error name>: <message>"
                sihd::util::Result<Message> call(Connection & connection, sihd::util::Duration timeout) const;
                sihd::util::Result<void> send(Connection & connection) const;
                // waits for an incoming message: nullopt once the timeout elapsed or connection.interrupt() was called
                static sihd::util::Result<std::optional<Message>> receive(Connection & connection,
                                                                          sihd::util::Duration timeout);

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
        bool is_open() const;
        void close();

        // dbus match rule, e.g. "type='signal',sender='org.bluez',member='InterfacesAdded'"
        sihd::util::Result<void> add_match(const std::string & rule);
        sihd::util::Result<void> remove_match(const std::string & rule);
        // wakes a Message::receive() blocked in another thread
        sihd::util::Result<void> interrupt();

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

using Message = Connection::Message;
using Iter = Connection::Message::Iter;

} // namespace sihd::sys::linux_::dbus

#endif
