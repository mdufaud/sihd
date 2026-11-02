#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include "Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

bool is_string_type(int type)
{
    return type == DBUS_TYPE_STRING || type == DBUS_TYPE_OBJECT_PATH || type == DBUS_TYPE_SIGNATURE;
}

bool is_container_type(int type)
{
    return type == DBUS_TYPE_ARRAY || type == DBUS_TYPE_STRUCT || type == DBUS_TYPE_VARIANT
           || type == DBUS_TYPE_DICT_ENTRY;
}

Error type_mismatch(int expected, int actual)
{
    return Error(invalid_argument,
                 "dbus type mismatch: expected '{}', got '{}'",
                 static_cast<char>(expected),
                 static_cast<char>(actual));
}

} // namespace

struct Iter::Impl
{
        ::DBusMessageIter iter {};
};

Iter::Iter(const Message & message)
{
    if (message._impl != nullptr)
    {
        _impl = std::make_unique<Impl>();
        dbus_message_iter_init(message._impl->message, &_impl->iter);
    }
}

char Iter::type() const
{
    if (_impl == nullptr)
        return '\0';
    return static_cast<char>(dbus_message_iter_get_arg_type(&_impl->iter));
}

std::string Iter::signature() const
{
    if (this->type() == '\0')
        return {};
    char *signature = dbus_message_iter_get_signature(&_impl->iter);
    std::string ret = string_or_empty(signature);
    dbus_free(signature);
    return ret;
}

bool Iter::next()
{
    if (_impl == nullptr)
        return false;
    return dbus_message_iter_next(&_impl->iter);
}

template <BasicType T>
Result<T> Iter::get() const
{
    const int actual = this->type();
    if constexpr (std::is_same_v<T, std::string>)
    {
        if (!is_string_type(actual))
            return std::unexpected(type_mismatch(DBUS_TYPE_STRING, actual));
        const char *value = nullptr;
        dbus_message_iter_get_basic(&_impl->iter, &value);
        return std::string(value);
    }
    else
    {
        constexpr int expected = dbus_type_of<T>();
        if (actual != expected)
            return std::unexpected(type_mismatch(expected, actual));

        if constexpr (std::is_same_v<T, bool>)
        {
            dbus_bool_t value = 0;
            dbus_message_iter_get_basic(&_impl->iter, &value);
            return value != 0;
        }
        else
        {
            T value {};
            dbus_message_iter_get_basic(&_impl->iter, &value);
            return value;
        }
    }
}

Result<Iter> Iter::recurse() const
{
    const int actual = this->type();
    if (!is_container_type(actual))
        return std::unexpected(Error(invalid_argument, "dbus type '{}' is not a container", static_cast<char>(actual)));
    auto sub = std::make_unique<Impl>();
    dbus_message_iter_recurse(&_impl->iter, &sub->iter);
    return Iter(std::move(sub));
}

Result<UnixFd> Iter::get_unix_fd() const
{
    const int actual = this->type();
    if (actual != DBUS_TYPE_UNIX_FD)
        return std::unexpected(type_mismatch(DBUS_TYPE_UNIX_FD, actual));
    int fd = -1;
    dbus_message_iter_get_basic(&_impl->iter, &fd);
    if (fd < 0)
        return std::unexpected(Error(io_error, "could not duplicate the dbus unix fd"));
    return UnixFd(fd);
}

# else

struct Iter::Impl
{
};

Iter::Iter(const Message &) {}

char Iter::type() const
{
    return '\0';
}

std::string Iter::signature() const
{
    return {};
}

bool Iter::next()
{
    return false;
}

template <BasicType T>
Result<T> Iter::get() const
{
    return unsupported_platform();
}

Result<Iter> Iter::recurse() const
{
    return unsupported_platform();
}

Result<UnixFd> Iter::get_unix_fd() const
{
    return unsupported_platform();
}

# endif

Iter::Iter(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Iter::Iter(Iter && other) = default;

Iter & Iter::operator=(Iter && other) = default;

Iter::~Iter() = default;

template <BasicType T>
Result<std::vector<T>> Iter::get_array() const
{
    auto items = this->recurse();
    SIHD_UNEXPECTED_RETURN(items);
    if (this->type() != 'a')
        return std::unexpected(Error(invalid_argument, "dbus type '{}' is not an array", this->type()));

    std::vector<T> values;
    for (; items->type() != '\0'; items->next())
    {
        auto value = items->get<T>();
        SIHD_UNEXPECTED_RETURN(value);
        values.emplace_back(std::move(*value));
    }
    return values;
}

template <BasicType Key>
Result<void> Iter::for_each_entry(const std::function<Result<void>(const Key &, Iter &)> & callback) const
{
    if (this->type() != 'a')
        return std::unexpected(Error(invalid_argument, "dbus type '{}' is not a dict", this->type()));
    auto entries = this->recurse();
    SIHD_UNEXPECTED_RETURN(entries);

    for (; entries->type() != '\0'; entries->next())
    {
        auto entry = entries->recurse();
        SIHD_UNEXPECTED_RETURN(entry);
        auto key = entry->get<Key>();
        SIHD_UNEXPECTED_RETURN(key);
        entry->next();

        auto value = entry->type() == 'v' ? entry->recurse() : Result<Iter>(std::move(*entry));
        SIHD_UNEXPECTED_RETURN(value);
        auto done = callback(*key, *value);
        SIHD_UNEXPECTED_RETURN(done);
    }
    return {};
}

# define SIHD_DBUS_INSTANTIATE(T)                                                                                      \
     template Result<T> Iter::get<T>() const;                                                                          \
     template Result<std::vector<T>> Iter::get_array<T>() const;                                                       \
     template Result<void> Iter::for_each_entry<T>(const std::function<Result<void>(const T &, Iter &)> &) const;
SIHD_DBUS_FOR_EACH_BASIC_TYPE(SIHD_DBUS_INSTANTIATE)
# undef SIHD_DBUS_INSTANTIATE

} // namespace sihd::sys::linux_::dbus

#endif
