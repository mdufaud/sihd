#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <fmt/format.h>

# include <sihd/sys/os.hpp>

# include "Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

Error signature_mismatch(const std::string & expected, const std::string & actual)
{
    return Error(invalid_argument, "dbus type mismatch: expected '{}', got '{}'", expected, actual);
}

std::string first_complete_type(const std::string & signature)
{
    ::DBusSignatureIter iter;
    dbus_signature_iter_init(&iter, signature.c_str());
    char *first = dbus_signature_iter_get_signature(&iter);
    std::string ret(first);
    dbus_free(first);
    return ret;
}

} // namespace

// libdbus aborts on what its writer does not expect: types and parent/child order are checked before each write
struct Appender::Impl
{
        ::DBusMessageIter iter {};
        Impl *parent = nullptr;
        Impl *child = nullptr;
        // a dropped open container leaves the whole message unusable
        bool abandoned = false;
        bool constrained = false;
        bool repeat = false;
        std::string expected;
        // a struct takes at least one field
        bool is_struct = false;
        bool has_values = false;
        // set on the root only: the message outlives its appenders
        bool *message_abandoned = nullptr;

        ~Impl()
        {
            if (parent != nullptr)
                this->abandon();
            else if (child != nullptr)
                child->abandon();
        }

        void abandon()
        {
            if (child != nullptr)
                child->abandon();
            dbus_message_iter_abandon_container_if_open(&parent->iter, &iter);
            for (Impl *ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent)
            {
                ancestor->abandoned = true;
                if (ancestor->message_abandoned != nullptr)
                    *ancestor->message_abandoned = true;
            }
            parent->child = nullptr;
            parent = nullptr;
        }

        Result<void> check_writable() const
        {
            if (abandoned)
                return std::unexpected(Error(invalid_argument, "dbus message has an abandoned container"));
            if (child != nullptr)
                return std::unexpected(Error(invalid_argument, "dbus container has an open child"));
            return {};
        }

        Result<std::string> next_type() const
        {
            auto writable = this->check_writable();
            SIHD_UNEXPECTED_RETURN(writable);
            if (!constrained)
                return std::string();
            if (expected.empty())
                return std::unexpected(Error(invalid_argument, "dbus container takes no more values"));
            return first_complete_type(expected);
        }

        Result<std::string> expect(const std::string & type) const
        {
            auto next = this->next_type();
            SIHD_UNEXPECTED_RETURN(next);
            if (!next->empty() && *next != type)
                return std::unexpected(signature_mismatch(*next, type));
            return next;
        }

        Result<std::string> expect_opening(char opening) const
        {
            auto next = this->next_type();
            SIHD_UNEXPECTED_RETURN(next);
            if (!next->empty() && !next->starts_with(opening))
                return std::unexpected(signature_mismatch(*next, std::string(1, opening)));
            return next;
        }

        void consume(const std::string & type)
        {
            has_values = true;
            if (!repeat)
                expected.erase(0, type.size());
        }

        Result<void> append(int type, const void *value)
        {
            auto next = this->expect(std::string(1, static_cast<char>(type)));
            SIHD_UNEXPECTED_RETURN(next);
            if (!dbus_message_iter_append_basic(&iter, type, value))
                return std::unexpected(Error(out_of_memory, "could not append dbus argument"));
            this->consume(*next);
            return {};
        }

        Result<Appender>
            open_child(int type, const char *signature, const std::string & consumed, std::unique_ptr<Impl> container)
        {
            if (!dbus_message_iter_open_container(&iter, type, signature, &container->iter))
                return std::unexpected(Error(out_of_memory, "could not open dbus container"));
            this->consume(consumed);
            container->parent = this;
            child = container.get();
            return Appender(std::move(container));
        }

        static std::unique_ptr<Impl> constrained_to(const std::string & expected, bool repeat)
        {
            auto container = std::make_unique<Impl>();
            container->constrained = true;
            container->repeat = repeat;
            container->expected = expected;
            return container;
        }
};

Appender::Appender(Message & message)
{
    if (message._impl != nullptr)
    {
        _impl = std::make_unique<Impl>();
        _impl->abandoned = message._impl->abandoned;
        _impl->message_abandoned = &message._impl->abandoned;
        dbus_message_iter_init_append(message._impl->message, &_impl->iter);
    }
}

struct Appender::Writer
{
        Appender & to;

        template <FixedType T>
        Result<void> operator()(const T & value) const
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                const dbus_bool_t dbus_value = value ? TRUE : FALSE;
                return to._impl->append(DBUS_TYPE_BOOLEAN, &dbus_value);
            }
            else
                return to._impl->append(dbus_type_of<T>(), &value);
        }

        Result<void> operator()(const std::string & value) const
        {
            if (!dbus_validate_utf8(value.c_str(), nullptr))
                return std::unexpected(Error(invalid_argument, "dbus string is not valid utf-8"));
            const char *str = value.c_str();
            return to._impl->append(DBUS_TYPE_STRING, &str);
        }

        Result<void> operator()(const Value::ObjectPath & path) const
        {
            if (!dbus_validate_path(path.str().c_str(), nullptr))
                return std::unexpected(Error(invalid_argument, "invalid dbus path: {}", path.str()));
            const char *str = path.str().c_str();
            return to._impl->append(DBUS_TYPE_OBJECT_PATH, &str);
        }

        Result<void> operator()(const Value::Signature & signature) const
        {
            if (!dbus_signature_validate(signature.str().c_str(), nullptr))
                return std::unexpected(Error(invalid_argument, "invalid dbus signature: {}", signature.str()));
            const char *str = signature.str().c_str();
            return to._impl->append(DBUS_TYPE_SIGNATURE, &str);
        }

        Result<void> operator()(const Value::Array & array) const
        {
            return this->fill(to.open_array(array.element_signature()), array.items());
        }

        Result<void> operator()(const Value::Struct & value) const
        {
            return this->fill(to.open_struct(), value.fields());
        }

        Result<void> operator()(const Value::DictEntry & entry) const
        {
            auto opened = to.open_dict_entry();
            SIHD_UNEXPECTED_RETURN(opened);
            auto added = opened->add(entry.key());
            SIHD_UNEXPECTED_RETURN(added);
            added = opened->add(entry.value());
            SIHD_UNEXPECTED_RETURN(added);
            return to.close(std::move(*opened));
        }

        Result<void> operator()(const Value::Variant & variant) const
        {
            auto opened = to.open_variant(variant.value().signature());
            SIHD_UNEXPECTED_RETURN(opened);
            auto added = opened->add(variant.value());
            SIHD_UNEXPECTED_RETURN(added);
            return to.close(std::move(*opened));
        }

        Result<void> fill(Result<Appender> container, const std::vector<Value> & items) const
        {
            SIHD_UNEXPECTED_RETURN(container);
            for (const Value & item : items)
            {
                auto added = container->add(item);
                SIHD_UNEXPECTED_RETURN(added);
            }
            return to.close(std::move(*container));
        }
};

Result<void> Appender::add(const Value & value)
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    return std::visit(Writer {*this}, value.data());
}

Result<void> Appender::add_unix_fd(int fd)
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    auto type = os::fd_type(fd);
    SIHD_UNEXPECTED_RETURN(type);
    return _impl->append(DBUS_TYPE_UNIX_FD, &fd);
}

Result<Appender> Appender::open_array(const std::string & element_signature)
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    const std::string type = fmt::format("a{}", element_signature);
    if (!dbus_signature_validate_single(type.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus array element signature: {}", element_signature));
    auto next = _impl->expect(type);
    SIHD_UNEXPECTED_RETURN(next);

    return _impl->open_child(DBUS_TYPE_ARRAY,
                             element_signature.c_str(),
                             *next,
                             Impl::constrained_to(element_signature, true));
}

Result<Appender> Appender::open_variant(const std::string & value_signature)
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    if (!dbus_signature_validate_single(value_signature.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus variant signature: {}", value_signature));
    auto next = _impl->expect("v");
    SIHD_UNEXPECTED_RETURN(next);

    return _impl->open_child(DBUS_TYPE_VARIANT,
                             value_signature.c_str(),
                             *next,
                             Impl::constrained_to(value_signature, false));
}

Result<Appender> Appender::open_struct()
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    auto next = _impl->expect_opening('(');
    SIHD_UNEXPECTED_RETURN(next);
    int depth = 1;
    for (const Impl *ancestor = _impl.get(); ancestor != nullptr; ancestor = ancestor->parent)
        depth += ancestor->is_struct ? 1 : 0;
    if (depth > DBUS_MAXIMUM_TYPE_RECURSION_DEPTH)
        return std::unexpected(
            Error(invalid_argument, "dbus structs nest at most {} deep", DBUS_MAXIMUM_TYPE_RECURSION_DEPTH));

    auto container = next->empty() ? std::make_unique<Impl>()
                                   : Impl::constrained_to(next->substr(1, next->size() - 2), false);
    container->is_struct = true;
    return _impl->open_child(DBUS_TYPE_STRUCT, nullptr, *next, std::move(container));
}

Result<Appender> Appender::open_dict_entry()
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    auto next = _impl->expect_opening('{');
    SIHD_UNEXPECTED_RETURN(next);
    if (next->empty())
        return std::unexpected(Error(invalid_argument, "a dbus dict entry only goes in an array of dict entries"));

    return _impl->open_child(DBUS_TYPE_DICT_ENTRY,
                             nullptr,
                             *next,
                             Impl::constrained_to(next->substr(1, next->size() - 2), false));
}

Result<void> Appender::close(Appender && container)
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    if (container._impl == nullptr || container._impl->parent != _impl.get())
        return std::unexpected(Error(invalid_argument, "dbus container was not opened by this appender"));
    auto writable = container._impl->check_writable();
    SIHD_UNEXPECTED_RETURN(writable);
    if (!container._impl->repeat && !container._impl->expected.empty())
        return std::unexpected(Error(invalid_argument, "dbus container misses '{}'", container._impl->expected));
    if (container._impl->is_struct && !container._impl->has_values)
        return std::unexpected(Error(invalid_argument, "dbus struct has no field"));

    std::unique_ptr<Impl> closed = std::move(container._impl);
    closed->parent = nullptr;
    _impl->child = nullptr;
    if (!dbus_message_iter_close_container(&_impl->iter, &closed->iter))
        return std::unexpected(Error(out_of_memory, "could not close dbus container"));
    return {};
}

# else

struct Appender::Impl
{
};

Appender::Appender(Message &) {}

Result<void> Appender::add(const Value &)
{
    return unsupported_platform();
}

Result<void> Appender::add_unix_fd(int)
{
    return unsupported_platform();
}

Result<Appender> Appender::open_array(const std::string &)
{
    return unsupported_platform();
}

Result<Appender> Appender::open_variant(const std::string &)
{
    return unsupported_platform();
}

Result<Appender> Appender::open_struct()
{
    return unsupported_platform();
}

Result<Appender> Appender::open_dict_entry()
{
    return unsupported_platform();
}

Result<void> Appender::close(Appender &&)
{
    return unsupported_platform();
}

# endif

Appender::Appender(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Appender::Appender(Appender && other) = default;

Appender & Appender::operator=(Appender && other) = default;

Appender::~Appender() = default;

} // namespace sihd::sys::linux_::dbus

#endif
