#include <cstdlib>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/CliInterpreter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerManager.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

static int eval(CliInterpreter & cli, std::vector<std::string> args)
{
    return cli.evaluate(args);
}

class TestCliInterpreter: public ::testing::Test
{
    protected:
        TestCliInterpreter() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCliInterpreter() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestCliInterpreter, test_evaluate_without_runtime)
{
    CliInterpreter cli("interp");
    bool ran = false;
    cli.root().add_command("serve", "serve").on_run([&] { ran = true; });

    EXPECT_EQ(eval(cli, {"serve"}), EXIT_SUCCESS);
    EXPECT_TRUE(ran);
    // evaluations are repeatable: no boot, no conf, no logging
    EXPECT_EQ(eval(cli, {"serve"}), EXIT_SUCCESS);
    EXPECT_TRUE(ran);
}

TEST_F(TestCliInterpreter, test_exit_status)
{
    CliInterpreter cli("interp");
    cli.root().add_command("quit", "quit").on_run([] { throw CliInterpreter::Exit(7); });
    EXPECT_EQ(eval(cli, {"quit"}), 7);
    // the evaluation state did not stick
    cli.root().add_command("ok", "ok").on_run([] {});
    EXPECT_EQ(eval(cli, {"ok"}), EXIT_SUCCESS);
}

TEST_F(TestCliInterpreter, test_help_interception)
{
    CliInterpreter cli("interp");
    cli.root().add_command("get", "get values");
    std::string name;
    cli.root().add_command("fill", "fills").bind_positional("name", name, "a name").on_run([&] { name = "filled"; });

    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(cli, {"help"}), EXIT_SUCCESS);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("get values"), std::string::npos);

    // help as a positional value is not a help request
    EXPECT_EQ(eval(cli, {"fill", "help"}), EXIT_SUCCESS);
    EXPECT_EQ(name, "filled");
}

TEST_F(TestCliInterpreter, test_completion)
{
    CliInterpreter cli("my-app");
    std::string url;
    cli.root().add_command("service", "manage services").add_command("add", "add").bind("url", url, "the url");

    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(cli, {"__complete", "service", "add", "--u"}), EXIT_SUCCESS);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("--url"), std::string::npos);

    // the static dump carries the nodes of the whole tree
    std::string script = cli.bash_completion();
    EXPECT_NE(script.find("complete -F _my_app -o default my-app"), std::string::npos);
    EXPECT_NE(script.find("\"service add\")"), std::string::npos);
}

} // namespace test
