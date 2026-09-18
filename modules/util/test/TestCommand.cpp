#include <cstdlib>

#include <gtest/gtest.h>

#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

class TestCommand: public ::testing::Test
{
    protected:
        TestCommand() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCommand() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

static int eval(CliApp & app, std::vector<std::string> args)
{
    return app.evaluate(args);
}

TEST_F(TestCommand, test_tree)
{
    CliApp app({.name = "tree"});

    Command & get = app.root().add_command("get", "get values");
    Command & stat = get.add_command("stat", "statistics");
    Command & service = stat.add_command("service", "service statistics");

    ASSERT_TRUE(app.root().has_command("get"));
    ASSERT_FALSE(app.root().has_command("put"));
    EXPECT_EQ(app.root().get_command("get"), &get);
    EXPECT_EQ(app.root().path("get.stat.service"), &service);
    EXPECT_EQ(app.root().path("get.stat.nope"), nullptr);
    EXPECT_EQ(app.root().path("get.nope.service"), nullptr);
    EXPECT_EQ(app.root().path("get"), &get);
    EXPECT_EQ(app.root().path(""), &app.root());

    const std::vector<std::string> get_children = get.command_names();
    ASSERT_EQ(get_children.size(), 1u);
    EXPECT_EQ(get_children[0], "stat");
    EXPECT_EQ(get.description(), "get values");
    EXPECT_EQ(stat.name(), "stat");

    EXPECT_TRUE(service.help().find("service statistics") != std::string::npos);
}

TEST_F(TestCommand, test_add_commands)
{
    CliApp app({.name = "bulk"});

    Command & cache = app.root().add_command("cache", "cache commands");
    cache.add_command("clear", "clear the cache");
    cache.add_command("stats", "cache statistics");

    EXPECT_TRUE(cache.has_command("clear"));
    EXPECT_TRUE(cache.has_command("stats"));
    EXPECT_EQ(cache.command_names().size(), 2u);
}

TEST_F(TestCommand, test_add_remove_live)
{
    CliApp app({.name = "live"});

    Command & service = app.root().add_command("service", "service management");

    Command & toto = service.add_command("toto", "toto service");
    toto.on_run([&app] { app.exit(3); });

    EXPECT_TRUE(service.has_command("toto"));
    EXPECT_EQ(eval(app, {"service", "toto"}), 3);

    EXPECT_TRUE(service.remove_command("toto"));
    EXPECT_FALSE(service.remove_command("toto"));
    EXPECT_FALSE(service.has_command("toto"));
    EXPECT_EQ(app.root().path("service.toto"), nullptr);
    EXPECT_NE(eval(app, {"service", "toto"}), 3);

    // a removed branch takes its children with it
    Command & nested = service.add_command("nested", "nested");
    nested.add_command("deep", "deep");
    EXPECT_TRUE(service.remove_command("nested"));
    EXPECT_EQ(app.root().path("service.nested.deep"), nullptr);

    service.add_command("dup", "dup");
    EXPECT_THROW(service.add_command("dup", "dup"), std::invalid_argument);
}

TEST_F(TestCommand, test_bind_vector_positional)
{
    CliApp app({.name = "multi"});
    std::vector<std::string> paths;
    app.root().bind_positional("paths", paths, "paths");
    app.root().on_run([&] { app.exit((int)paths.size()); });

    EXPECT_EQ(eval(app, {"a", "b", "c"}), 3);
    // the cli feeds the same target: an evaluation without arguments leaves it untouched
    EXPECT_EQ(eval(app, {}), 3);
    // a conf or --set application replaces the whole list: only the cli appends
    EXPECT_EQ(eval(app, {"--set", "paths=x"}), 1);
}

TEST_F(TestCommand, test_bind_aliases)
{
    CliApp app({.name = "aliases"});
    std::string key;
    int port = 0;
    app.root().bind("key", key, "a key", "k");
    app.root().bind("port", port, "a port", "p");
    app.root().on_run([] {});

    // short aliases feed the same targets as the long names
    EXPECT_EQ(eval(app, {"-k", "file.pem", "-p", "1234"}), EXIT_SUCCESS);
    EXPECT_EQ(key, "file.pem");
    EXPECT_EQ(port, 1234);

    EXPECT_EQ(eval(app, {"--key", "other.pem"}), EXIT_SUCCESS);
    EXPECT_EQ(key, "other.pem");

    // aliases show up in the help
    std::string help = app.root().help();
    EXPECT_NE(help.find("-k"), std::string::npos);
    EXPECT_NE(help.find("--key"), std::string::npos);
    EXPECT_NE(help.find("-p"), std::string::npos);
}

TEST_F(TestCommand, test_positional_subcommand_chain)
{
    CliApp app({.name = "chain"});
    int port = 0;
    std::string host;
    std::vector<std::string> words;

    Command & port_cmd = app.root().add_command("port", "port to use");
    port_cmd.bind_positional("value", port, "port value");
    Command & server_cmd = port_cmd.add_command("server", "server to reach");
    server_cmd.bind_positional("host", host, "host name");
    server_cmd.add_command("send", "send words").bind_positional("words", words, "words to send").on_run([&] {
        app.exit((int)words.size());
    });

    // each level consumes its own positional then delegates the rest: send collects all remaining tokens
    EXPECT_EQ(eval(app, {"port", "4242", "server", "localhost", "send", "toto", "titi", "tata"}), 3);
    EXPECT_EQ(port, 4242);
    EXPECT_EQ(host, "localhost");
}

TEST_F(TestCommand, test_mount_copy)
{
    CliApp app({.name = "mount"});
    int port = 0;
    std::string host;
    std::vector<std::string> words;

    Command & port_cmd = app.root().add_command("port", "port to use");
    port_cmd.bind_positional("value", port, "port value");
    port_cmd.on_run([&] { app.exit(port); });
    Command & server_cmd = app.root().add_command("server", "server to reach");
    server_cmd.bind_positional("host", host, "host name");

    // send is written once, then both subtrees are mounted under each other
    server_cmd.add_command("send", "send words").bind_positional("words", words, "words to send").on_run([&] {
        app.exit((int)words.size());
    });

    server_cmd.add_command(port_cmd); // port is copied before it gets children
    port_cmd.add_command(server_cmd); // the server copy carries send

    ASSERT_NE(app.root().path("port.server.send"), nullptr);
    ASSERT_NE(app.root().path("server.port"), nullptr);

    EXPECT_EQ(eval(app, {"port", "4242", "server", "localhost", "send", "a", "b", "c"}), 3);
    EXPECT_EQ(port, 4242);
    // host was written through the server copy mounted under port
    EXPECT_EQ(host, "localhost");

    // the mounted port feeds the same bound variable and runs the original callback
    EXPECT_EQ(eval(app, {"server", "srv", "port", "1234"}), 1234);
    EXPECT_EQ(port, 1234);

    EXPECT_THROW(port_cmd.add_command(server_cmd), std::invalid_argument);

    // removing one mount leaves the other intact
    EXPECT_TRUE(port_cmd.remove_command("server"));
    EXPECT_EQ(app.root().path("port.server.send"), nullptr);
    EXPECT_EQ(eval(app, {"server", "srv", "send", "d"}), 4);
    // the removed mount no longer runs its callback: dispatch stays on port (4242) while the
    // fallthrough still parses the remaining tokens into the bound variables
    EXPECT_NE(eval(app, {"port", "4242", "server", "localhost", "send", "a", "b", "c"}), 3);
}

TEST_F(TestCommand, test_mount_recursion_refused)
{
    CliApp app({.name = "rec"});
    Command & outer = app.root().add_command("outer", "outer");
    Command & inner = outer.add_command("inner", "inner");

    EXPECT_THROW(outer.add_command(outer), std::logic_error);
    EXPECT_THROW(inner.add_command(outer), std::logic_error);
}

} // namespace test
