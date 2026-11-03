#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/SecretStore.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

namespace
{

constexpr const char *session_path = "/org/freedesktop/secrets/session/s1";
constexpr const char *collection_path = "/org/freedesktop/secrets/collection/login";
constexpr const char *item_path = "/org/freedesktop/secrets/collection/login/1";
constexpr const char *prompt_path = "/org/freedesktop/secrets/prompt/p1";
constexpr const char *no_object = "/";

std::map<std::string, std::string> read_attributes(const Iter & attributes)
{
    std::map<std::string, std::string> values;
    auto entries = attributes.recurse();
    if (!entries.has_value())
        return values;
    for (; entries->type() != '\0'; entries->next())
    {
        auto entry = entries->recurse();
        if (!entry.has_value())
            return values;
        const std::string key = entry->get<std::string>().value_or("unreadable");
        entry->next();
        values[key] = entry->get<std::string>().value_or("unreadable");
    }
    return values;
}

struct CreateItem
{
        std::string label;
        std::map<std::string, std::string> attributes;
        std::string session;
        std::vector<uint8_t> parameters;
        std::vector<uint8_t> value;
        std::string content_type;
        bool replace = false;
};

Result<CreateItem> read_create_item(const Message & call)
{
    CreateItem item;
    Iter args(call);
    auto read = args.for_each_entry<std::string>([&item](const std::string & key, Iter & value) -> Result<void> {
        if (key == "org.freedesktop.Secret.Item.Label")
            item.label = value.get<std::string>().value_or("unreadable");
        else if (key == "org.freedesktop.Secret.Item.Attributes")
            item.attributes = read_attributes(value);
        return {};
    });
    SIHD_UNEXPECTED_RETURN(read);
    args.next();

    auto secret = args.recurse();
    SIHD_UNEXPECTED_RETURN(secret);
    item.session = secret->get<std::string>().value_or("unreadable");
    secret->next();
    item.parameters = secret->get_array<uint8_t>().value_or(std::vector<uint8_t> {1});
    secret->next();
    item.value = secret->get_array<uint8_t>().value_or(std::vector<uint8_t> {});
    secret->next();
    item.content_type = secret->get<std::string>().value_or("unreadable");
    args.next();
    item.replace = args.get<bool>().value_or(false);
    return item;
}

} // namespace

class TestSecretService: public ::testing::Test
{
    protected:
        TestSecretService() { LoggerManager::stream(); }

        virtual ~TestSecretService() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!SecretStore::supported)
            {
                GTEST_SKIP() << "the secret store is not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto opened = _server.open(_bus.address());
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
            auto owned = _server.request_name("org.freedesktop.secrets");
            ASSERT_TRUE(owned.has_value()) << owned.error().message;
        }

        void start_client(const std::vector<std::string> & args)
        {
            std::vector<std::string> argv {"secret", "service=mail", "account=me@example.org"};
            argv.insert(argv.end(), args.begin(), args.end());
            ASSERT_TRUE(_client.start(_bus.address(), argv));
        }

        Message expect_call(const std::string & member)
        {
            auto call = receive_member(_server, member);
            EXPECT_TRUE(call.has_value()) << "no call of " << member;
            if (!call.has_value())
                throw std::runtime_error("no call of " + member);
            return std::move(*call);
        }

        void send(Message & reply) { ASSERT_TRUE(reply.send(_server).has_value()); }

        Message new_reply(const Message & call)
        {
            auto reply = call.method_return();
            EXPECT_TRUE(reply.has_value());
            if (!reply.has_value())
                throw std::runtime_error("no reply");
            return std::move(*reply);
        }

        void answer_open_session()
        {
            Message call = this->expect_call("OpenSession");
            EXPECT_EQ(call.signature(), "sv");
            EXPECT_EQ(Iter(call).get<std::string>().value_or("unreadable"), "plain");
            Message reply = this->new_reply(call);
            Appender body(reply);
            auto output = body.open_variant("s");
            ASSERT_TRUE(output.has_value());
            ASSERT_TRUE(output->add("").has_value());
            ASSERT_TRUE(body.close(std::move(*output)).has_value());
            ASSERT_TRUE(body.add_object_path(session_path).has_value());
            this->send(reply);
        }

        void add_paths(Appender & body, const std::vector<std::string> & paths)
        {
            auto array = body.open_array("o");
            ASSERT_TRUE(array.has_value());
            for (const std::string & path : paths)
                ASSERT_TRUE(array->add_object_path(path).has_value());
            ASSERT_TRUE(body.close(std::move(*array)).has_value());
        }

        void answer_search(const std::vector<std::string> & unlocked, const std::vector<std::string> & locked)
        {
            Message call = this->expect_call("SearchItems");
            EXPECT_EQ(read_attributes(Iter(call)),
                      (std::map<std::string, std::string> {{"service", "mail"}, {"username", "me@example.org"}}));
            Message reply = this->new_reply(call);
            Appender body(reply);
            this->add_paths(body, unlocked);
            this->add_paths(body, locked);
            this->send(reply);
        }

        void answer_unlock(const std::vector<std::string> & expected, const std::string & prompt)
        {
            Message call = this->expect_call("Unlock");
            EXPECT_EQ(Iter(call).get_array<std::string>().value_or(std::vector<std::string> {}), expected);
            Message reply = this->new_reply(call);
            Appender body(reply);
            this->add_paths(body, prompt == no_object ? expected : std::vector<std::string> {});
            ASSERT_TRUE(body.add_object_path(prompt).has_value());
            this->send(reply);
        }

        void answer_prompt_completed(bool dismissed)
        {
            Message call = this->expect_call("Prompt");
            EXPECT_EQ(call.path(), prompt_path);
            Message reply = this->new_reply(call);
            this->send(reply);

            auto signal = Message::signal(prompt_path, "org.freedesktop.Secret.Prompt", "Completed");
            ASSERT_TRUE(signal.has_value());
            Appender body(*signal);
            ASSERT_TRUE(body.add(dismissed).has_value());
            auto result = body.open_variant("s");
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(result->add("").has_value());
            ASSERT_TRUE(body.close(std::move(*result)).has_value());
            ASSERT_TRUE(signal->send(_server).has_value());
        }

        void answer_get_secret(const std::string & secret)
        {
            Message call = this->expect_call("GetSecret");
            EXPECT_EQ(call.path(), item_path);
            EXPECT_EQ(Iter(call).get<std::string>().value_or("unreadable"), session_path);
            Message reply = this->new_reply(call);
            Appender body(reply);
            auto value = body.open_struct();
            ASSERT_TRUE(value.has_value());
            ASSERT_TRUE(value->add_object_path(session_path).has_value());
            ASSERT_TRUE(value->add_array(std::vector<uint8_t> {}).has_value());
            ASSERT_TRUE(value->add_array(std::vector<uint8_t>(secret.begin(), secret.end())).has_value());
            ASSERT_TRUE(value->add("text/plain").has_value());
            ASSERT_TRUE(body.close(std::move(*value)).has_value());
            this->send(reply);
        }

        void answer_read_alias(const std::string & collection)
        {
            Message call = this->expect_call("ReadAlias");
            EXPECT_EQ(Iter(call).get<std::string>().value_or("unreadable"), "default");
            Message reply = this->new_reply(call);
            ASSERT_TRUE(reply.add_object_path(collection).has_value());
            this->send(reply);
        }

        void answer_two_paths(const Message & call, const std::string & first, const std::string & second)
        {
            Message reply = this->new_reply(call);
            ASSERT_TRUE(reply.add_object_path(first).has_value());
            ASSERT_TRUE(reply.add_object_path(second).has_value());
            this->send(reply);
        }

        PrivateDBus _bus;
        Connection _server;
        PrivateDBusClient _client;
};

TEST_F(TestSecretService, test_store)
{
    this->start_client({"action=store", "secret=hunter2"});
    this->answer_open_session();
    this->answer_read_alias(collection_path);
    this->answer_unlock({collection_path}, no_object);

    Message call = this->expect_call("CreateItem");
    EXPECT_EQ(call.path(), collection_path);
    EXPECT_EQ(call.signature(), "a{sv}(oayays)b");

    auto item = read_create_item(call);
    ASSERT_TRUE(item.has_value()) << item.error().message;
    this->answer_two_paths(call, item_path, no_object);

    EXPECT_EQ(_client.next_line(), "ok");
    EXPECT_EQ(_client.finish(), 0);
    EXPECT_EQ(item->label, "me@example.org@mail");
    EXPECT_EQ(item->attributes,
              (std::map<std::string, std::string> {{"service", "mail"}, {"username", "me@example.org"}}));
    EXPECT_EQ(item->session, session_path);
    EXPECT_EQ(item->parameters, std::vector<uint8_t> {});
    EXPECT_EQ(std::string(item->value.begin(), item->value.end()), "hunter2");
    EXPECT_EQ(item->content_type, "text/plain");
    EXPECT_TRUE(item->replace);
}

TEST_F(TestSecretService, test_store_without_default_collection)
{
    this->start_client({"action=store", "secret=hunter2"});
    this->answer_open_session();
    this->answer_read_alias(no_object);

    EXPECT_EQ(_client.next_line(), error_line(ErrorCode::not_found, "the keyring has no default collection"));
}

TEST_F(TestSecretService, test_lookup)
{
    this->start_client({"action=lookup"});
    this->answer_open_session();
    this->answer_search({item_path}, {});
    this->answer_get_secret("hunter2");

    EXPECT_EQ(_client.next_line(), "secret=hunter2");
    EXPECT_EQ(_client.finish(), 0);
}

TEST_F(TestSecretService, test_lookup_not_found)
{
    this->start_client({"action=lookup"});
    this->answer_open_session();
    this->answer_search({}, {});

    EXPECT_EQ(_client.next_line(),
              error_line(ErrorCode::not_found, "no secret for account 'me@example.org' of service 'mail'"));
}

TEST_F(TestSecretService, test_lookup_unlocks_through_a_prompt)
{
    this->start_client({"action=lookup", "prompt_ms=5000"});
    this->answer_open_session();
    this->answer_search({}, {item_path});
    this->answer_unlock({item_path}, prompt_path);
    this->answer_prompt_completed(false);
    this->answer_search({item_path}, {});
    this->answer_get_secret("hunter2");

    EXPECT_EQ(_client.next_line(), "secret=hunter2");
    EXPECT_EQ(_client.finish(), 0);
}

TEST_F(TestSecretService, test_lookup_prompt_not_allowed)
{
    this->start_client({"action=lookup"});
    this->answer_open_session();
    this->answer_search({}, {item_path});
    this->answer_unlock({item_path}, prompt_path);

    EXPECT_EQ(_client.next_line(),
              error_line(ErrorCode::permission_denied, "the keyring needs a prompt, none is allowed"));
}

TEST_F(TestSecretService, test_lookup_prompt_dismissed)
{
    this->start_client({"action=lookup", "prompt_ms=5000"});
    this->answer_open_session();
    this->answer_search({}, {item_path});
    this->answer_unlock({item_path}, prompt_path);
    this->answer_prompt_completed(true);

    EXPECT_EQ(_client.next_line(), error_line(ErrorCode::permission_denied, "the keyring prompt was dismissed"));
}

TEST_F(TestSecretService, test_lookup_prompt_times_out_and_gets_dismissed)
{
    this->start_client({"action=lookup", "prompt_ms=50"});
    this->answer_open_session();
    this->answer_search({}, {item_path});
    this->answer_unlock({item_path}, prompt_path);
    Message prompt = this->expect_call("Prompt");
    Message prompted = this->new_reply(prompt);
    this->send(prompted);

    Message dismiss = this->expect_call("Dismiss");
    EXPECT_EQ(dismiss.path(), prompt_path);
    Message dismissed = this->new_reply(dismiss);
    this->send(dismissed);

    EXPECT_EQ(_client.next_line(), error_line(ErrorCode::timeout, "dbus receive timed out"));
}

TEST_F(TestSecretService, test_remove)
{
    this->start_client({"action=remove"});
    this->answer_open_session();
    this->answer_search({item_path}, {});

    Message call = this->expect_call("Delete");
    EXPECT_EQ(call.path(), item_path);
    EXPECT_EQ(call.interface(), "org.freedesktop.Secret.Item");
    Message reply = this->new_reply(call);
    ASSERT_TRUE(reply.add_object_path(no_object).has_value());
    this->send(reply);

    EXPECT_EQ(_client.next_line(), "ok");
    EXPECT_EQ(_client.finish(), 0);
}

} // namespace test
