#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <cstdint>
# include <optional>
# include <utility>
# include <vector>

# include <fmt/format.h>

# include <sihd/sys/linux/dbus.hpp>

# include "internal/desktop_bus.hpp"
#endif

#include <sihd/sys/SecretStore.hpp>

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace dbus = sihd::sys::linux_::dbus;

namespace
{

constexpr ZStringView secrets_service = "org.freedesktop.secrets";
constexpr ZStringView secrets_path = "/org/freedesktop/secrets";
constexpr ZStringView service_interface = "org.freedesktop.Secret.Service";
constexpr ZStringView collection_interface = "org.freedesktop.Secret.Collection";
constexpr ZStringView item_interface = "org.freedesktop.Secret.Item";
constexpr ZStringView prompt_interface = "org.freedesktop.Secret.Prompt";
constexpr ZStringView no_object = "/";

struct ItemSearch
{
        std::vector<std::string> unlocked;
        std::vector<std::string> locked;
};

// the attribute names python-keyring writes: secret-tool and keyring read the same secrets
Result<void> append_attributes(dbus::Appender & parent, ZStringView service, ZStringView account)
{
    auto attributes = parent.open_array("{ss}");
    SIHD_UNEXPECTED_RETURN(attributes);
    for (const auto & [key, value] : {std::pair<ZStringView, ZStringView> {"service", service},
                                      std::pair<ZStringView, ZStringView> {"username", account}})
    {
        auto entry = attributes->open_dict_entry();
        SIHD_UNEXPECTED_RETURN(entry);
        auto added = entry->add(key);
        SIHD_UNEXPECTED_RETURN(added);
        added = entry->add(value);
        SIHD_UNEXPECTED_RETURN(added);
        added = attributes->close(std::move(*entry));
        SIHD_UNEXPECTED_RETURN(added);
    }
    return parent.close(std::move(*attributes));
}

Result<void> append_object_paths(dbus::Appender & parent, const std::vector<std::string> & paths)
{
    auto array = parent.open_array("o");
    SIHD_UNEXPECTED_RETURN(array);
    for (const std::string & path : paths)
    {
        auto added = array->add_object_path(path);
        SIHD_UNEXPECTED_RETURN(added);
    }
    return parent.close(std::move(*array));
}

} // namespace

struct SecretStore::Impl
{
        // a failed call drops the connection: the next one reconnects to a restarted bus or keyring
        Result<dbus::Message> call(const dbus::Message & request)
        {
            auto reply = request.call(connection, internal::dbus_call_timeout);
            if (!reply.has_value())
            {
                connection.close();
                session.clear();
            }
            return reply;
        }

        Result<void> open_session()
        {
            if (!connection.is_open())
            {
                session.clear();
                auto opened = connection.open(dbus::Bus::session);
                SIHD_UNEXPECTED_RETURN(opened);
            }
            if (!session.empty())
                return {};

            dbus::Message request;
            auto built = request.method_call(secrets_service, secrets_path, service_interface, "OpenSession");
            SIHD_UNEXPECTED_RETURN(built);
            dbus::Appender body(request);
            auto added = body.add(ZStringView("plain"));
            SIHD_UNEXPECTED_RETURN(added);
            auto input = body.open_variant("s");
            SIHD_UNEXPECTED_RETURN(input);
            added = input->add(ZStringView(""));
            SIHD_UNEXPECTED_RETURN(added);
            added = body.close(std::move(*input));
            SIHD_UNEXPECTED_RETURN(added);

            auto reply = this->call(request);
            SIHD_UNEXPECTED_RETURN(reply);
            dbus::Iter output(*reply);
            output.next();
            auto path = output.get<std::string>();
            SIHD_UNEXPECTED_RETURN(path);
            session = std::move(*path);
            return {};
        }

        Result<void> dismiss_prompt(ZStringView prompt)
        {
            dbus::Message request;
            auto built = request.method_call(secrets_service, prompt, prompt_interface, "Dismiss");
            SIHD_UNEXPECTED_RETURN(built);
            auto reply = this->call(request);
            SIHD_UNEXPECTED_RETURN(reply);
            return {};
        }

        Result<void> complete_prompt(ZStringView prompt)
        {
            if (prompt == no_object)
                return {};
            if (!prompt_timeout.has_value())
                return std::unexpected(Error(permission_denied, "the keyring needs a prompt, none is allowed"));

            const std::string rule = dbus::signal_rule(secrets_service, prompt, prompt_interface, "Completed");
            auto matched = connection.add_match(rule);
            SIHD_UNEXPECTED_RETURN(matched);
            dbus::Message request;
            auto built = request.method_call(secrets_service, prompt, prompt_interface, "Prompt");
            SIHD_UNEXPECTED_RETURN(built);
            auto added = request.add(ZStringView(""));
            SIHD_UNEXPECTED_RETURN(added);
            auto reply = this->call(request);
            SIHD_UNEXPECTED_RETURN(reply);

            dbus::Message completion;
            auto completed = completion.receive_until(connection,
                                                      *prompt_timeout,
                                                      [prompt](const dbus::Message & message) -> Result<bool> {
                                                          return message.member() == "Completed"
                                                                 && message.path() == prompt.view();
                                                      });
            (void)connection.remove_match(rule);
            if (!completed.has_value())
                (void)this->dismiss_prompt(prompt);
            SIHD_UNEXPECTED_RETURN(completed);

            auto dismissed = dbus::Iter(completion).get<bool>();
            SIHD_UNEXPECTED_RETURN(dismissed);
            if (*dismissed)
                return std::unexpected(Error(permission_denied, "the keyring prompt was dismissed"));
            return {};
        }

        Result<void> unlock(const std::vector<std::string> & paths)
        {
            dbus::Message request;
            auto built = request.method_call(secrets_service, secrets_path, service_interface, "Unlock");
            SIHD_UNEXPECTED_RETURN(built);
            dbus::Appender body(request);
            auto added = append_object_paths(body, paths);
            SIHD_UNEXPECTED_RETURN(added);
            auto reply = this->call(request);
            SIHD_UNEXPECTED_RETURN(reply);
            dbus::Iter output(*reply);
            output.next();
            auto prompt = output.get<std::string>();
            SIHD_UNEXPECTED_RETURN(prompt);
            return this->complete_prompt(*prompt);
        }

        Result<ItemSearch> search(ZStringView service, ZStringView account)
        {
            dbus::Message request;
            auto built = request.method_call(secrets_service, secrets_path, service_interface, "SearchItems");
            SIHD_UNEXPECTED_RETURN(built);
            dbus::Appender body(request);
            auto added = append_attributes(body, service, account);
            SIHD_UNEXPECTED_RETURN(added);

            auto reply = this->call(request);
            SIHD_UNEXPECTED_RETURN(reply);
            dbus::Iter output(*reply);
            auto unlocked = output.get_array<std::string>();
            SIHD_UNEXPECTED_RETURN(unlocked);
            output.next();
            auto locked = output.get_array<std::string>();
            SIHD_UNEXPECTED_RETURN(locked);
            return ItemSearch {std::move(*unlocked), std::move(*locked)};
        }

        Result<std::vector<std::string>> unlocked_items(ZStringView service, ZStringView account)
        {
            auto opened = this->open_session();
            SIHD_UNEXPECTED_RETURN(opened);
            auto found = this->search(service, account);
            SIHD_UNEXPECTED_RETURN(found);
            if (found->locked.empty())
                return std::move(found->unlocked);
            auto unlocked = this->unlock(found->locked);
            SIHD_UNEXPECTED_RETURN(unlocked);
            auto found_again = this->search(service, account);
            SIHD_UNEXPECTED_RETURN(found_again);
            return std::move(found_again->unlocked);
        }

        Result<std::string> default_collection()
        {
            dbus::Message request;
            auto built = request.method_call(secrets_service, secrets_path, service_interface, "ReadAlias");
            SIHD_UNEXPECTED_RETURN(built);
            auto added = request.add(ZStringView("default"));
            SIHD_UNEXPECTED_RETURN(added);
            auto reply = this->call(request);
            SIHD_UNEXPECTED_RETURN(reply);
            auto path = dbus::Iter(*reply).get<std::string>();
            SIHD_UNEXPECTED_RETURN(path);
            if (*path == no_object.view())
                return std::unexpected(Error(not_found, "the keyring has no default collection"));
            return path;
        }

        Result<void> append_secret(dbus::Appender & parent, std::string_view secret) const
        {
            auto value = parent.open_struct();
            SIHD_UNEXPECTED_RETURN(value);
            auto added = value->add_object_path(session);
            SIHD_UNEXPECTED_RETURN(added);
            added = value->add_array(std::vector<uint8_t> {});
            SIHD_UNEXPECTED_RETURN(added);
            added = value->add_array(std::vector<uint8_t>(secret.begin(), secret.end()));
            SIHD_UNEXPECTED_RETURN(added);
            added = value->add(ZStringView("text/plain"));
            SIHD_UNEXPECTED_RETURN(added);
            return parent.close(std::move(*value));
        }

        dbus::Connection connection;
        std::string session;
        std::optional<Duration> prompt_timeout;
};

Result<void> SecretStore::store(ZStringView service, ZStringView account, std::string_view secret)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    valid = SecretStore::check_secret(secret);
    SIHD_UNEXPECTED_RETURN(valid);
    valid = _impl->open_session();
    SIHD_UNEXPECTED_RETURN(valid);
    auto collection = _impl->default_collection();
    SIHD_UNEXPECTED_RETURN(collection);
    valid = _impl->unlock({*collection});
    SIHD_UNEXPECTED_RETURN(valid);

    dbus::Message request;
    auto built = request.method_call(secrets_service, *collection, collection_interface, "CreateItem");
    SIHD_UNEXPECTED_RETURN(built);

    dbus::Appender body(request);
    auto properties = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(properties);
    auto added = properties->add_entry("org.freedesktop.Secret.Item.Label",
                                       "s",
                                       [&service, &account](dbus::Appender & value) {
                                           return value.add(ZStringView(fmt::format("{}@{}", account, service)));
                                       });
    SIHD_UNEXPECTED_RETURN(added);
    added = properties->add_entry(
        "org.freedesktop.Secret.Item.Attributes",
        "a{ss}",
        [&service, &account](dbus::Appender & value) { return append_attributes(value, service, account); });
    SIHD_UNEXPECTED_RETURN(added);
    added = body.close(std::move(*properties));
    SIHD_UNEXPECTED_RETURN(added);

    added = _impl->append_secret(body, secret);
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(true);
    SIHD_UNEXPECTED_RETURN(added);

    auto reply = _impl->call(request);
    SIHD_UNEXPECTED_RETURN(reply);
    dbus::Iter output(*reply);
    output.next();
    auto prompt = output.get<std::string>();
    SIHD_UNEXPECTED_RETURN(prompt);
    return _impl->complete_prompt(*prompt);
}

Result<std::string> SecretStore::lookup(ZStringView service, ZStringView account)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    auto items = _impl->unlocked_items(service, account);
    SIHD_UNEXPECTED_RETURN(items);
    if (items->empty())
        return std::unexpected(SecretStore::secret_not_found(service, account));

    dbus::Message request;
    auto built = request.method_call(secrets_service, items->front(), item_interface, "GetSecret");
    SIHD_UNEXPECTED_RETURN(built);
    auto added = request.add_object_path(_impl->session);
    SIHD_UNEXPECTED_RETURN(added);
    auto reply = _impl->call(request);
    SIHD_UNEXPECTED_RETURN(reply);
    // (session, parameters, value, content type)
    auto fields = dbus::Iter(*reply).recurse();
    SIHD_UNEXPECTED_RETURN(fields);
    fields->next();
    fields->next();
    auto value = fields->get_array<uint8_t>();
    SIHD_UNEXPECTED_RETURN(value);
    return std::string(value->begin(), value->end());
}

Result<void> SecretStore::remove(ZStringView service, ZStringView account)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    auto items = _impl->unlocked_items(service, account);
    SIHD_UNEXPECTED_RETURN(items);
    if (items->empty())
        return std::unexpected(SecretStore::secret_not_found(service, account));

    for (const std::string & item : *items)
    {
        dbus::Message request;
        auto built = request.method_call(secrets_service, item, item_interface, "Delete");
        SIHD_UNEXPECTED_RETURN(built);
        auto reply = _impl->call(request);
        SIHD_UNEXPECTED_RETURN(reply);
        auto prompt = dbus::Iter(*reply).get<std::string>();
        SIHD_UNEXPECTED_RETURN(prompt);
        auto completed = _impl->complete_prompt(*prompt);
        SIHD_UNEXPECTED_RETURN(completed);
    }
    return {};
}

void SecretStore::set_prompt_timeout(Duration timeout)
{
    _impl->prompt_timeout = timeout;
}

#else

struct SecretStore::Impl
{
};

Result<void> SecretStore::store(ZStringView service, ZStringView account, std::string_view secret)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    valid = SecretStore::check_secret(secret);
    SIHD_UNEXPECTED_RETURN(valid);
    return std::unexpected(Error(not_supported, "the secret store is not supported on this platform"));
}

Result<std::string> SecretStore::lookup(ZStringView service, ZStringView account)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    return std::unexpected(Error(not_supported, "the secret store is not supported on this platform"));
}

Result<void> SecretStore::remove(ZStringView service, ZStringView account)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    return std::unexpected(Error(not_supported, "the secret store is not supported on this platform"));
}

void SecretStore::set_prompt_timeout(Duration) {}

#endif

SecretStore::SecretStore(): _impl(std::make_unique<Impl>()) {}

SecretStore::SecretStore(SecretStore && other) = default;

SecretStore & SecretStore::operator=(SecretStore && other) = default;

SecretStore::~SecretStore() = default;

} // namespace sihd::sys
