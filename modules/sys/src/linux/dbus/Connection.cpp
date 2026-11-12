#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include "Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace
{

constexpr ZStringView bus_driver = "org.freedesktop.DBus";

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

using MatchFunction = void (*)(::DBusConnection *, const char *, ::DBusError *);

Result<void> apply_match(::DBusConnection *connection, ZStringView rule, MatchFunction function)
{
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    function(connection, rule.c_str(), &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    return {};
}

# endif

Result<void> read_credential(Credentials & credentials, const std::string & key, Iter & value)
{
    if (key == "UnixUserID")
    {
        auto uid = value.get<uint32_t>();
        SIHD_UNEXPECTED_RETURN(uid);
        credentials.user = user::UserId::from_native(*uid);
    }
    if (key == "ProcessID")
    {
        auto pid = value.get<uint32_t>();
        SIHD_UNEXPECTED_RETURN(pid);
        credentials.pid = static_cast<pid_t>(*pid);
    }
    if (key == "UnixGroupIDs")
    {
        auto gids = value.get_array<uint32_t>();
        SIHD_UNEXPECTED_RETURN(gids);
        for (const uint32_t gid : *gids)
            credentials.groups.emplace_back(user::GroupId::from_native(gid));
    }
    return {};
}

} // namespace

Connection::Connection() = default;

Connection::Connection(Connection && other) = default;

Connection & Connection::operator=(Connection && other) = default;

Connection::~Connection() = default;

bool Connection::is_open() const
{
    return _impl != nullptr;
}

void Connection::close()
{
    _impl.reset();
}

void Connection::disable_auto_start()
{
    _auto_start = false;
}

Result<Credentials> Connection::credentials(ZStringView bus_name, Duration timeout)
{
    Message request;
    auto built = request.method_call(bus_driver, "/org/freedesktop/DBus", bus_driver, "GetConnectionCredentials");
    SIHD_UNEXPECTED_RETURN(built);
    auto name_added = request.add(bus_name);
    SIHD_UNEXPECTED_RETURN(name_added);
    auto reply = request.call(*this, timeout);
    SIHD_UNEXPECTED_RETURN(reply);

    Credentials credentials;
    auto walked = Iter(*reply).for_each_entry<std::string>(
        [&credentials](const std::string & key, Iter & value) { return read_credential(credentials, key, value); });
    SIHD_UNEXPECTED_RETURN(walked);
    return credentials;
}

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

Connection::Impl::~Impl()
{
    if (connection == nullptr)
        return;
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
}

Result<void> Connection::Impl::adopt(::DBusConnection *opened, ::DBusError & dbus_error)
{
    if (opened == nullptr)
        return std::unexpected(dbus_error_take(dbus_error));
    // libdbus calls _exit() on a lost bus otherwise
    dbus_connection_set_exit_on_disconnect(opened, FALSE);
    connection = opened;

    int fd = -1;
    if (!dbus_connection_get_unix_fd(connection, &fd))
        return std::unexpected(Error(io_error, "dbus connection has no unix socket"));
    auto wake_opened = wake.open();
    SIHD_UNEXPECTED_RETURN(wake_opened);
    auto queued_opened = queued.open();
    SIHD_UNEXPECTED_RETURN(queued_opened);
    poll.set_read_fd(fd);
    poll.set_read_fd(wake.fd());
    poll.set_read_fd(queued.fd());
    return {};
}

Result<void> Connection::open(Bus bus)
{
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    auto impl = std::make_unique<Impl>();
    auto adopted = impl->adopt(
        dbus_bus_get_private(bus == Bus::system ? DBUS_BUS_SYSTEM : DBUS_BUS_SESSION, &dbus_error),
        dbus_error);
    SIHD_UNEXPECTED_RETURN(adopted);
    _impl = std::move(impl);
    return {};
}

Result<void> Connection::open(ZStringView address)
{
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    auto impl = std::make_unique<Impl>();
    auto adopted = impl->adopt(dbus_connection_open_private(address.c_str(), &dbus_error), dbus_error);
    SIHD_UNEXPECTED_RETURN(adopted);
    if (!dbus_bus_register(impl->connection, &dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    _impl = std::move(impl);
    return {};
}

Result<void> Connection::_check_name(ZStringView name) const
{
    if (_impl == nullptr)
        return std::unexpected(connection_not_open());
    if (!dbus_validate_bus_name(name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus name: {}", name));
    return {};
}

Result<NameRequest> Connection::request_name(ZStringView name, const NameOptions & options)
{
    auto checked = this->_check_name(name);
    SIHD_UNEXPECTED_RETURN(checked);
    unsigned int flags = 0;
    if (options.allow_replacement)
        flags |= DBUS_NAME_FLAG_ALLOW_REPLACEMENT;
    if (options.replace_existing)
        flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;
    if (!options.queue)
        flags |= DBUS_NAME_FLAG_DO_NOT_QUEUE;
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const int reply = dbus_bus_request_name(_impl->connection, name.c_str(), flags, &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    if (reply == DBUS_REQUEST_NAME_REPLY_IN_QUEUE)
        return NameRequest::queued;
    if (reply != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER && reply != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)
        return std::unexpected(Error(already_exists, "dbus name {} is owned by another connection", name));
    return NameRequest::primary_owner;
}

Result<void> Connection::release_name(ZStringView name)
{
    auto checked = this->_check_name(name);
    SIHD_UNEXPECTED_RETURN(checked);
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const int reply = dbus_bus_release_name(_impl->connection, name.c_str(), &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    if (reply == DBUS_RELEASE_NAME_REPLY_NON_EXISTENT)
        return std::unexpected(Error(not_found, "dbus name {} has no owner", name));
    if (reply == DBUS_RELEASE_NAME_REPLY_NOT_OWNER)
        return std::unexpected(Error(invalid_argument, "dbus name {} is owned by another connection", name));
    return {};
}

Result<bool> Connection::name_has_owner(ZStringView name)
{
    auto checked = this->_check_name(name);
    SIHD_UNEXPECTED_RETURN(checked);
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const dbus_bool_t owned = dbus_bus_name_has_owner(_impl->connection, name.c_str(), &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    return owned != 0;
}

Result<void> Connection::add_match(ZStringView rule)
{
    if (_impl == nullptr)
        return std::unexpected(connection_not_open());
    return apply_match(_impl->connection, rule, dbus_bus_add_match);
}

Result<void> Connection::remove_match(ZStringView rule)
{
    if (_impl == nullptr)
        return std::unexpected(connection_not_open());
    return apply_match(_impl->connection, rule, dbus_bus_remove_match);
}

Result<void> Connection::interrupt()
{
    if (_impl == nullptr)
        return std::unexpected(connection_not_open());
    return _impl->wake.notify();
}

# else

Result<void> Connection::open(Bus)
{
    return unsupported_platform();
}

Result<void> Connection::open(ZStringView)
{
    return unsupported_platform();
}

Result<NameRequest> Connection::request_name(ZStringView, const NameOptions &)
{
    return unsupported_platform();
}

Result<void> Connection::release_name(ZStringView)
{
    return unsupported_platform();
}

Result<bool> Connection::name_has_owner(ZStringView)
{
    return unsupported_platform();
}

Result<void> Connection::add_match(ZStringView)
{
    return unsupported_platform();
}

Result<void> Connection::remove_match(ZStringView)
{
    return unsupported_platform();
}

Result<void> Connection::interrupt()
{
    return unsupported_platform();
}

# endif

} // namespace sihd::sys::linux_::dbus

#endif
