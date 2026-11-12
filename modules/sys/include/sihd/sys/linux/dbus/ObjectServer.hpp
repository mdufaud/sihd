#ifndef __SIHD_SYS_LINUX_DBUS_OBJECTSERVER_HPP__
#define __SIHD_SYS_LINUX_DBUS_OBJECTSERVER_HPP__

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <sihd/sys/linux/dbus/Appender.hpp>
#include <sihd/sys/linux/dbus/Connection.hpp>
#include <sihd/sys/linux/dbus/Iter.hpp>
#include <sihd/sys/linux/dbus/Message.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::linux_::dbus
{

// answers the method calls of its objects from the loop calling Message::receive(), on that thread only
class ObjectServer
{
    private:
        // only the server builds a Call: nobody else can name this
        struct Token
        {
                explicit Token() = default;
        };

    public:
        // one answer per method call, later from any thread once moved out of the handler, within the connection's life
        class Call
        {
            public:
                Call(Token token, Connection & connection, Message && message);
                Call(Call && other);
                // a pending call dropped here answers Failed, the caller never waits for its timeout
                ~Call();

                Call(const Call &) = delete;
                Call & operator=(const Call &) = delete;
                Call & operator=(Call &&) = delete;

                const Message & message() const;
                bool pending() const;
                // nothing leaves when the caller expects no reply
                sihd::util::Result<void> send(const Message & reply);
                sihd::util::Result<void> fail(const sihd::util::Error & error);

                // the easy way for one argument and one value, Iter and method_return() with send() for the others
                template <BasicType T>
                sihd::util::Result<T> argument() const;
                sihd::util::Result<void> reply();
                template <BasicType T>
                sihd::util::Result<void> reply(const T & value);

            private:
                Connection *_connection;
                Message _message;
                bool _answered = false;
        };

        using MethodHandler = std::function<sihd::util::Result<void>(Call &)>;

        struct Arg
        {
                std::string name;
                std::string signature;
        };

        struct Method
        {
                std::string name;
                std::vector<Arg> in;
                std::vector<Arg> out;
                MethodHandler handler;
        };

        struct Signal
        {
                std::string name;
                std::vector<Arg> args;
        };

        struct Property
        {
                std::string name;
                std::string signature;
                // add_object and remove_object refuse while it runs: GetAll and GetManagedObjects walk the objects
                std::function<sihd::util::Result<void>(Appender &)> get;
                std::function<sihd::util::Result<void>(Iter &)> set;
                bool emits_changed = true;

                // the easy way for one value: the signature follows T
                template <BasicType T>
                static Property read_only(const std::string & name, std::function<T()> getter);
                // reads and writes value, which outlives the object
                template <BasicType T>
                static Property bound(const std::string & name, T & value);
        };

        struct Interface
        {
                std::string name;
                std::vector<Method> methods;
                std::vector<Property> properties;
                std::vector<Signal> signals;
        };

        explicit ObjectServer(Connection & connection);

        ObjectServer(const ObjectServer &) = delete;
        ObjectServer & operator=(const ObjectServer &) = delete;

        sihd::util::Result<void> add_object(const std::string & path, std::vector<Interface> interfaces);
        sihd::util::Result<void> remove_object(const std::string & path);
        sihd::util::Result<void> add_object_manager(const std::string & path);
        sihd::util::Result<void> remove_object_manager(const std::string & path);

        // answers a method call, gives back any other message (signals, replies, NameLost...)
        sihd::util::Result<std::optional<Message>> handle(Message && message);
        // sends request and answers the calls its peer makes meanwhile, other messages are dropped until the reply
        sihd::util::Result<Message> call(const Message & request, sihd::util::Duration timeout);

        sihd::util::Result<void> emit_properties_changed(const std::string & path,
                                                         const std::string & interface,
                                                         const std::vector<std::string> & names);

    private:
        sihd::util::Result<void> _serve(Call & call);
        sihd::util::Result<void> _serve_method(Call & call, const std::vector<Interface> & interfaces);
        sihd::util::Result<void> _serve_properties(Call & call, const std::vector<Interface> & interfaces);
        sihd::util::Result<void> _serve_property_set(Call & call, const std::vector<Interface> & interfaces);
        sihd::util::Result<void> _serve_managed_objects(Call & call);
        sihd::util::Result<void> _emit_interfaces_added(const std::string & path);
        sihd::util::Result<void> _emit_interfaces_removed(const std::string & path);
        std::vector<std::string> _managers_of(const std::string & path) const;
        std::vector<std::string> _children_of(const std::string & path) const;
        std::string _introspect(const std::string & path) const;
        sihd::util::Defer _read_scope();

        Connection & _connection;
        std::map<std::string, std::vector<Interface>> _objects;
        std::set<std::string> _managers;
        bool _reading = false;
};

} // namespace sihd::sys::linux_::dbus

#endif
