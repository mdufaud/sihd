#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/util/CliApp.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerConsole.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

static int run_app(CliApp & app, const std::vector<std::string> & args)
{
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>("prog"));
    for (const std::string & arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    return app.run((int)argv.size(), argv.data());
}

static int eval(CliApp & app, std::vector<std::string> args)
{
    return app.evaluate(args);
}

static void set_env(const char *name, const char *value)
{
#if defined(__SIHD_WINDOWS__)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static void unset_env(const char *name)
{
#if defined(__SIHD_WINDOWS__)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

// sends the default logging to stdout; the base install_logging applies the level filter
class TestLogApp: public CliApp
{
    public:
        using CliApp::CliApp;

    protected:
        ALogger *create_default_logger() override { return new LoggerStream(stdout); }
};

// records every lifecycle hook with the state it ran in
class HookApp: public CliApp
{
    public:
        using CliApp::CliApp;

        std::vector<std::string> hooks;

    protected:
        void on_boot() override { hooks.push_back("boot:" + this->state_str()); }
        void on_configure() override { hooks.push_back("configure:" + this->state_str()); }
        void on_run() override { hooks.push_back("run:" + this->state_str()); }
        void on_terminate() override { hooks.push_back("terminate:" + this->state_str()); }
        void on_halt() override { hooks.push_back("halt:" + this->state_str()); }
        void on_fail() override { hooks.push_back("fail:" + this->state_str()); }
};

// keeps the base logging setup, exposing what install_logging installed
class LogSpyApp: public CliApp
{
    public:
        using CliApp::CliApp;

        using CliApp::install_logging;
        using CliApp::logger;
};

class TestCliApp: public ::testing::Test
{
    protected:
        TestCliApp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCliApp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestCliApp, test_evaluate_dispatch)
{
    CliApp app({.name = "eval", .setup_logging = false});
    app.root().add_command("serve", "serve").on_run([] { throw CliApp::Exit(7); });

    EXPECT_EQ(eval(app, {"serve"}), 7);
    // evaluation can be repeated without keeping parse state
    EXPECT_EQ(eval(app, {"serve"}), 7);

    // a node without on_run prints its help
    app.root().add_command("info", "info");
    EXPECT_EQ(eval(app, {"info"}), EXIT_SUCCESS);
    // unknown command
    EXPECT_NE(eval(app, {"nope"}), EXIT_SUCCESS);
    // unknown option
    EXPECT_NE(eval(app, {"--nope", "serve"}), EXIT_SUCCESS);
}

TEST_F(TestCliApp, test_evaluate_string_quotes)
{
    CliApp app({.name = "quotes", .setup_logging = false});
    std::string name;
    app.root().add_command("get", "get").bind_positional("name", name, "a name").on_run([&] {
        if (name != "mon service")
            app.exit(EXIT_FAILURE);
    });

    EXPECT_EQ(app.evaluate("get \"mon service\""), EXIT_SUCCESS);
    EXPECT_EQ(app.evaluate("get 'mon service'"), EXIT_SUCCESS);
    // the escape char only protects enclosures, not the delimiter
    EXPECT_EQ(app.evaluate("get \"mon\\ service\""), EXIT_SUCCESS);
    EXPECT_NE(app.evaluate("get mon service"), EXIT_SUCCESS);
}

TEST_F(TestCliApp, test_precedence_struct_json)
{
    struct
    {
            std::string host = "localhost";
            int port = 8080;
            bool secure = false;
    } opts;

    CliApp app({.name = "prec", .setup_logging = false});
    std::string result;
    Command & serve = app.root().add_command("serve", "serve");
    serve.bind("host", opts.host, "");
    serve.bind("port", opts.port, "");
    serve.bind("secure", opts.secure, "");
    serve.on_run([&] { result = fmt::format("{}:{}{}", opts.host, opts.port, opts.secure ? "s" : ""); });
    app.set_conf_loader([] { return std::string(R"({"serve": {"host": "confhost", "port": 9090}})"); });

    ASSERT_EQ(run_app(app, {"serve"}), 0);
    EXPECT_EQ(result, "confhost:9090");
}

TEST_F(TestCliApp, test_precedence_cli_over_json)
{
    struct
    {
            std::string host = "localhost";
            int port = 8080;
    } opts;

    CliApp app({.name = "prec", .setup_logging = false});
    std::string result;
    Command & serve = app.root().add_command("serve", "serve");
    serve.bind("host", opts.host, "");
    serve.bind("port", opts.port, "");
    serve.on_run([&] { result = fmt::format("{}:{}", opts.host, opts.port); });
    app.set_conf_loader([] { return std::string(R"({"serve": {"host": "confhost", "port": 9090}})"); });

    ASSERT_EQ(run_app(app, {"serve", "--port", "1234"}), 0);
    // json still applies where cli did not speak
    EXPECT_EQ(result, "confhost:1234");
}

TEST_F(TestCliApp, test_precedence_set_overrides)
{
    struct
    {
            std::string host = "localhost";
            int port = 8080;
            bool secure = false;
    } opts;

    CliApp app({.name = "prec", .setup_logging = false});
    std::string result;
    Command & serve = app.root().add_command("serve", "serve");
    serve.bind("host", opts.host, "");
    serve.bind("port", opts.port, "");
    serve.bind("secure", opts.secure, "");
    serve.on_run([&] { result = fmt::format("{}:{}{}", opts.host, opts.port, opts.secure ? "s" : ""); });
    app.set_conf_loader([] { return std::string(R"({"serve": {"host": "confhost", "port": 9090}})"); });

    ASSERT_EQ(run_app(app, {"--set", "serve.port=7777", "--set", "serve.secure=true", "serve"}), 0);
    EXPECT_EQ(result, "confhost:7777s");
}

TEST_F(TestCliApp, test_conf_and_set_errors)
{
    struct
    {
            int port = 8080;
    } opts;

    CliApp app({.name = "err", .setup_logging = false});
    Command & serve = app.root().add_command("serve", "serve");
    serve.bind("port", opts.port, "");
    serve.on_run([] {});

    // unknown conf key
    app.set_conf_loader([] { return std::string(R"({"serve": {"nope": 1}})"); });
    EXPECT_EQ(run_app(app, {"serve"}), EXIT_FAILURE);
    EXPECT_EQ(app.state(), CliApp::State::error);

    // unknown --set key
    CliApp app2({.name = "err", .setup_logging = false});
    app2.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([] {});
    EXPECT_EQ(run_app(app2, {"--set", "serve.nope=1", "serve"}), EXIT_FAILURE);

    // --set value of the wrong type
    CliApp app3({.name = "err", .setup_logging = false});
    app3.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([] {});
    EXPECT_EQ(run_app(app3, {"--set", "serve.port=abc", "serve"}), EXIT_FAILURE);
}

TEST_F(TestCliApp, test_help_interception)
{
    CliApp app({.name = "help", .setup_logging = false});
    app.root().add_command("get", "get values").add_command("stat", "statistics");

    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"help"}), EXIT_SUCCESS);
    std::string root_help = testing::internal::GetCapturedStdout();
    EXPECT_NE(root_help.find("get values"), std::string::npos);

    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"get", "stat", "help"}), EXIT_SUCCESS);
    std::string stat_help = testing::internal::GetCapturedStdout();
    EXPECT_NE(stat_help.find("statistics"), std::string::npos);
    EXPECT_EQ(stat_help.find("get values"), std::string::npos);
}

TEST_F(TestCliApp, test_help_interception_option_value)
{
    CliApp app({.name = "helpval", .setup_logging = false});
    std::string conf;
    std::string ran;
    app.root().bind("conf", conf, "conf path");
    app.root().add_command("stat", "statistics").on_run([&] { ran = "stat"; });

    // "help" consumed as an option value is not the help command
    EXPECT_EQ(eval(app, {"--conf", "help", "stat"}), EXIT_SUCCESS);
    EXPECT_EQ(conf, "help");
    EXPECT_EQ(ran, "stat");

    conf.clear();
    EXPECT_EQ(eval(app, {"--conf=help", "stat"}), EXIT_SUCCESS);
    EXPECT_EQ(conf, "help");
    EXPECT_EQ(ran, "stat");

    // a standalone help token still prints the help
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"help"}), EXIT_SUCCESS);
    std::string root_help = testing::internal::GetCapturedStdout();
    EXPECT_NE(root_help.find("statistics"), std::string::npos);
}

TEST_F(TestCliApp, test_help_interception_positional)
{
    CliApp app({.name = "poshelp", .setup_logging = false});
    std::string name;
    bool ran = false;
    app.root().add_command("get", "get values").bind_positional("name", name, "a name").on_run([&] {
        ran = true;
        if (name != "help")
            app.exit(EXIT_FAILURE);
    });

    // "help" as a positional value is not a help request
    EXPECT_EQ(eval(app, {"get", "help"}), EXIT_SUCCESS);
    EXPECT_TRUE(ran);
}

TEST_F(TestCliApp, test_option_value_dash_prefix)
{
    CliApp app({.name = "dashval", .setup_logging = false});
    std::string conf;
    bool ran = false;
    app.root().bind("conf", conf, "conf path");
    app.root().add_command("stat", "statistics").on_run([&] { ran = true; });

    // a dash-prefixed token is consumed as an option value, exactly like CLI11
    EXPECT_EQ(eval(app, {"--conf", "--nope", "stat"}), EXIT_SUCCESS);
    EXPECT_EQ(conf, "--nope");
    EXPECT_TRUE(ran);
}

struct CompletionVars
{
        std::string name;
        std::string url;
        bool force = false;
        std::string removed;
};

static void add_completion_commands(CliApp & app, CompletionVars & vars)
{
    Command & service = app.root().add_command("service", "manage services");
    service.bind("name", vars.name, "the service name").bind("force", vars.force, "force the action");
    service.add_command("add", "add a service").bind("url", vars.url, "the service url");
    service.add_command("remove", "remove a service").bind_positional("name", vars.removed, "the service to remove");
}

// the candidates the completion of the static script computes for words, the last one being completed
static std::string complete_with_bash(const std::string & script, const std::vector<std::string> & words)
{
    const char *script_path = "/tmp/sihd_cli_completion_test.sh";
    const char *driver_path = "/tmp/sihd_cli_completion_test_driver.sh";
    {
        std::ofstream file(script_path);
        file << script;
    }
    std::string quoted;
    for (const std::string & word : words)
        quoted += fmt::format("'{}' ", word);
    {
        std::ofstream file(driver_path);
        file << fmt::format("source '{}'\nCOMP_WORDS=({})\nCOMP_CWORD={}\n_my_app\n"
                            "[[ ${{#COMPREPLY[@]}} -gt 0 ]] && printf '%s\\n' \"${{COMPREPLY[@]}}\"\n",
                            script_path,
                            quoted,
                            words.size() - 1);
    }
    std::string out;
    FILE *pipe = popen(fmt::format("bash {}", driver_path).c_str(), "r");
    if (pipe == nullptr)
        return "";
    char buffer[512];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        out.append(buffer, read);
    pclose(pipe);
    return out;
}

// the candidates of a __complete call, trailing empty lines removed
static std::vector<std::string> complete_lines(CliApp & app, const std::vector<std::string> & args)
{
    testing::internal::CaptureStdout();
    EXPECT_EQ(app.evaluate(args), EXIT_SUCCESS);
    std::string out = testing::internal::GetCapturedStdout();
    std::vector<std::string> lines = str::split(out, '\n');
    while (lines.empty() == false && lines.back().empty())
        lines.pop_back();
    return lines;
}

TEST_F(TestCliApp, test_completion_dump)
{
    CompletionVars vars;
    CliApp app({.name = "my-app", .description = "an app", .version = "1.0", .setup_logging = false});
    add_completion_commands(app, vars);

    // the static dump is the default and registers the sanitized function name
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"--generate-bash-completion"}), EXIT_SUCCESS);
    std::string script = testing::internal::GetCapturedStdout();
    EXPECT_NE(script.find("complete -F _my_app -o default my-app"), std::string::npos);
    EXPECT_NE(script.find("__my_app_node"), std::string::npos);
    EXPECT_NE(script.find("__my_app_values"), std::string::npos);
    EXPECT_NE(script.find("commands=\"service help\""), std::string::npos);

    // a node line carries its options plus the ancestors', fallthrough
    std::string add_line;
    for (const std::string & line : str::split(script, '\n'))
    {
        if (line.find("\"service add\")") != std::string::npos)
            add_line = line;
    }
    EXPECT_NE(add_line.find("--url"), std::string::npos);
    EXPECT_NE(add_line.find("--log-level"), std::string::npos);
    EXPECT_NE(add_line.find("--set"), std::string::npos);

    // the dynamic loader queries __complete
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"--generate-bash-completion=dynamic"}), EXIT_SUCCESS);
    std::string loader = testing::internal::GetCapturedStdout();
    EXPECT_NE(loader.find("complete -F _my_app -o default my-app"), std::string::npos);
    EXPECT_NE(loader.find("__complete"), std::string::npos);
    EXPECT_EQ(loader.find("__my_app_node"), std::string::npos);

    // an unknown kind fails
    EXPECT_EQ(eval(app, {"--generate-bash-completion=zsh"}), EXIT_FAILURE);

    // the dump wins at any depth and snapshots the whole tree
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"service", "add", "--generate-bash-completion"}), EXIT_SUCCESS);
    std::string deep = testing::internal::GetCapturedStdout();
    EXPECT_EQ(deep, script);

    // an empty program name produces no script
    CliApp nameless({.name = "", .setup_logging = false});
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(nameless, {"--generate-bash-completion"}), EXIT_SUCCESS);
    EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
}

TEST_F(TestCliApp, test_completion_static_script)
{
    if (std::system("command -v bash > /dev/null 2>&1") != 0)
        GTEST_SKIP() << "bash not available";
    CompletionVars vars;
    CliApp app({.name = "my-app", .description = "an app", .version = "1.0", .setup_logging = false});
    add_completion_commands(app, vars);

    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"--generate-bash-completion"}), EXIT_SUCCESS);
    std::string script = testing::internal::GetCapturedStdout();
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app, {"--generate-bash-completion=dynamic"}), EXIT_SUCCESS);
    std::string loader = testing::internal::GetCapturedStdout();

    {
        std::ofstream file("/tmp/sihd_cli_completion_loader_test.sh");
        file << loader;
    }
    EXPECT_EQ(std::system("bash -n /tmp/sihd_cli_completion_loader_test.sh"), 0);

    // subcommands and help
    EXPECT_EQ(complete_with_bash(script, {"my-app", ""}), "service\nhelp\n");
    // prefix filtering
    EXPECT_EQ(complete_with_bash(script, {"my-app", "ser"}), "service\n");
    // options and flags on a dash, including the cli11 help option
    EXPECT_EQ(complete_with_bash(script, {"my-app", "--"}), "--log-level\n--set\n--help\n--version\n");
    // depth: own options plus the ancestors', fallthrough
    EXPECT_EQ(complete_with_bash(script, {"my-app", "service", "add", "--u"}), "--url\n");
    // a value option consumes the next word
    EXPECT_EQ(complete_with_bash(script, {"my-app", "--log-level", "debug", "ser"}), "service\n");
    // known option values, plain and inline
    EXPECT_EQ(complete_with_bash(script, {"my-app", "--log-level", "de"}), "debug\n");
    EXPECT_EQ(complete_with_bash(script, {"my-app", "--log-level=de"}), "--log-level=debug\n");
    // unknown values fall back to nothing
    EXPECT_EQ(complete_with_bash(script, {"my-app", "--set", ""}), "");
    // a positional node offers no candidates
    EXPECT_EQ(complete_with_bash(script, {"my-app", "service", "remove", ""}), "");
    // a positional value keeps the node context: the node options are still offered after it
    std::string after_positional = complete_with_bash(script, {"my-app", "service", "remove", "foo", "--"});
    EXPECT_NE(after_positional.find("--name"), std::string::npos);
    EXPECT_NE(after_positional.find("--force"), std::string::npos);
    // an option of a sibling node is not offered
    EXPECT_EQ(after_positional.find("--url"), std::string::npos);
    // a positional node still offers no command after its value
    EXPECT_EQ(complete_with_bash(script, {"my-app", "service", "remove", "foo", ""}), "");
    // after -- everything is positional
    EXPECT_EQ(complete_with_bash(script, {"my-app", "--", ""}), "");
}

TEST_F(TestCliApp, test_completion_dynamic_candidates)
{
    CompletionVars vars;
    CliApp app({.name = "my-app", .description = "an app", .version = "1.0", .setup_logging = false});
    add_completion_commands(app, vars);

    // root commands and the help token
    EXPECT_EQ(complete_lines(app, {"__complete", ""}), (std::vector<std::string> {"service", "help"}));
    EXPECT_EQ(complete_lines(app, {"__complete", "ser"}), (std::vector<std::string> {"service"}));
    // options on a dash, version among the flags
    EXPECT_EQ(complete_lines(app, {"__complete", "--v"}), (std::vector<std::string> {"--version"}));
    // depth: own options plus the root ones, fallthrough
    std::vector<std::string> add_opts = complete_lines(app, {"__complete", "service", "add", "--"});
    EXPECT_NE(std::find(add_opts.begin(), add_opts.end(), "--url"), add_opts.end());
    EXPECT_NE(std::find(add_opts.begin(), add_opts.end(), "--set"), add_opts.end());
    // known values of --log-level
    EXPECT_EQ(
        complete_lines(app, {"__complete", "--log-level", ""}),
        (std::vector<std::string> {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"}));
    EXPECT_EQ(complete_lines(app, {"__complete", "--log-level", "de"}), (std::vector<std::string> {"debug"}));
    EXPECT_EQ(complete_lines(app, {"__complete", "--log-level=de"}), (std::vector<std::string> {"--log-level=debug"}));
    // unknown values yield nothing
    EXPECT_EQ(complete_lines(app, {"__complete", "--set", "x"}), (std::vector<std::string> {}));
    // a consumed value returns to the node candidates
    std::vector<std::string> after_value = complete_lines(app, {"__complete", "--log-level", "debug", ""});
    EXPECT_NE(std::find(after_value.begin(), after_value.end(), "service"), after_value.end());
    // a positional node offers no commands
    EXPECT_EQ(complete_lines(app, {"__complete", "service", "remove", ""}), (std::vector<std::string> {}));
    // after -- nothing is offered
    EXPECT_EQ(complete_lines(app, {"__complete", "--", ""}), (std::vector<std::string> {}));
    // live branches are seen
    app.root().add_command("late", "added later");
    EXPECT_EQ(complete_lines(app, {"__complete", "la"}), (std::vector<std::string> {"late"}));
}

TEST_F(TestCliApp, test_completion_interception_edges)
{
    // a real __complete command shadows the hidden one
    CliApp app({.name = "shadow", .setup_logging = false});
    bool ran = false;
    app.root().add_command("__complete", "own").on_run([&] { ran = true; });
    EXPECT_EQ(eval(app, {"__complete"}), EXIT_SUCCESS);
    EXPECT_TRUE(ran);

    // the dump flag consumed as an option value is not a dump request
    CliApp app2({.name = "my-app", .setup_logging = false});
    std::string conf;
    app2.root().bind("conf", conf, "a conf path");
    app2.root().add_command("service", "manage services");
    testing::internal::CaptureStdout();
    EXPECT_EQ(eval(app2, {"--conf=--generate-bash-completion", "service"}), EXIT_SUCCESS);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out.find("complete -F"), std::string::npos);
    EXPECT_EQ(conf, "--generate-bash-completion");
}

TEST_F(TestCliApp, test_options_scope_and_fallthrough)
{
    struct
    {
            int port = 1;
    } opts;

    CliApp app({.name = "scope", .setup_logging = false});
    app.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([] {});

    // --port only exists below serve
    EXPECT_NE(eval(app, {"--port", "1"}), EXIT_SUCCESS);
    // sihd options stay reachable at any depth
    EXPECT_EQ(eval(app, {"serve", "--log-level", "debug"}), EXIT_SUCCESS);
}

TEST_F(TestCliApp, test_conf_loader_error)
{
    CliApp app({.name = "loaderr", .setup_logging = false});
    bool ran = false;
    app.root().add_command("serve", "serve").on_run([&] { ran = true; });
    app.set_conf_loader([]() -> std::string { throw std::runtime_error("cannot read conf"); });

    EXPECT_EQ(run_app(app, {"serve"}), EXIT_FAILURE);
    EXPECT_EQ(app.state(), CliApp::State::error);
    EXPECT_FALSE(ran);
}

TEST_F(TestCliApp, test_reload)
{
    struct
    {
            int port = 8080;
    } opts;

    CliApp app({.name = "reload", .setup_logging = false});
    int reloads = 0;
    std::string conf = R"({"serve": {"port": 9}})";
    app.on_reload([&](void) { reloads++; });
    app.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([] {});
    app.set_conf_loader([&conf] { return conf; });

    // no reload without a loader: a no-op succeeds
    CliApp noloader({.name = "nol", .setup_logging = false});
    EXPECT_TRUE(noloader.reload());

    // cli wins over the conf, and a cli value given at boot is kept across reloads
    ASSERT_EQ(run_app(app, {"serve", "--port", "5"}), 0);
    EXPECT_EQ(opts.port, 5);
    ASSERT_TRUE(app.reload());
    EXPECT_EQ(opts.port, 5);
    EXPECT_EQ(reloads, 1);

    conf = R"({"serve": {"port": 11}})";
    ASSERT_TRUE(app.reload());
    // --set/--cli are boot-time: the conf replay does not override the cli value
    EXPECT_EQ(opts.port, 5);
    EXPECT_EQ(reloads, 2);

    // an empty conf is a no-op as well
    conf = "";
    ASSERT_TRUE(app.reload());
    EXPECT_EQ(opts.port, 5);
    EXPECT_EQ(reloads, 2);
}

TEST_F(TestCliApp, test_json_conf_over_cli_after_reload_without_cli)
{
    struct
    {
            int port = 8080;
    } opts;

    CliApp app({.name = "reload", .setup_logging = false});
    std::string conf = R"({"serve": {"port": 9}})";
    app.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([] {});
    app.set_conf_loader([&conf] { return conf; });

    ASSERT_EQ(run_app(app, {"serve"}), 0);
    EXPECT_EQ(opts.port, 9);
    conf = R"({"serve": {"port": 11}})";
    ASSERT_TRUE(app.reload());
    EXPECT_EQ(opts.port, 11);
}

TEST_F(TestCliApp, test_set_override_survives_reload)
{
    struct
    {
            int port = 8080;
    } opts;

    CliApp app({.name = "setreload", .setup_logging = false});
    std::string conf = R"({"serve": {"port": 9}})";
    app.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([] {});
    app.set_conf_loader([&conf] { return conf; });

    // --set is the strongest override at boot
    ASSERT_EQ(run_app(app, {"--set", "serve.port=7777", "serve"}), 0);
    EXPECT_EQ(opts.port, 7777);

    // the conf replay must not revert it
    conf = R"({"serve": {"port": 11}})";
    ASSERT_TRUE(app.reload());
    EXPECT_EQ(opts.port, 7777);
}

TEST_F(TestCliApp, test_wait_for_termination_timeout)
{
    CliApp app({.name = "wait", .setup_logging = false});
    app.stop();

    // already stopped: returns immediately
    auto start0 = std::chrono::steady_clock::now();
    app.wait_for_termination(std::nullopt);
    auto ms0 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start0).count();
    EXPECT_LT(ms0, 50);

    CliApp app2({.name = "wait", .setup_logging = false});
    auto start = std::chrono::steady_clock::now();
    app2.wait_for_termination(Duration(time::milli(200)));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_GE(ms, 150);
    EXPECT_FALSE(app2.should_stop());
}

TEST_F(TestCliApp, test_stop_from_thread)
{
    HookApp app({.name = "stop", .setup_logging = false});
    std::thread thread([&app] {
        time::msleep(150);
        app.stop();
    });
    app.wait_for_termination(std::nullopt);
    thread.join();
    EXPECT_TRUE(app.should_stop());
    // stop() only requests termination: no boot, no hook, no state change
    EXPECT_TRUE(app.hooks.empty());
    EXPECT_EQ(app.state(), CliApp::State::none);
}

TEST_F(TestCliApp, test_loop_step)
{
    CliApp app({.name = "loop", .setup_logging = false});
    int rounds = 0;
    app.loop(Duration(time::milli(1)), [&rounds] { return ++rounds < 3; });
    EXPECT_EQ(rounds, 3);
    EXPECT_FALSE(app.should_stop());
}

TEST_F(TestCliApp, test_states_and_observer)
{
    CliApp app({.name = "states", .setup_logging = false});
    std::vector<CliApp::State> states;
    Handler<CliApp *> handler([&](CliApp *observed) { states.push_back(observed->state()); });
    app.add_observer(&handler);
    app.root().add_command("serve", "serve").on_run([] {});

    ASSERT_EQ(run_app(app, {"serve"}), 0);
    ASSERT_EQ(states.size(), 5u);
    EXPECT_EQ(states[0], CliApp::State::loading);
    EXPECT_EQ(states[1], CliApp::State::configured);
    EXPECT_EQ(states[2], CliApp::State::running);
    EXPECT_EQ(states[3], CliApp::State::stopping);
    EXPECT_EQ(states[4], CliApp::State::stopped);
    EXPECT_EQ(app.state(), CliApp::State::stopped);
    EXPECT_EQ(app.state_str(), "stopped");
    app.remove_observer(&handler);
}

TEST_F(TestCliApp, test_lifecycle_hooks)
{
    HookApp app({.name = "hooks", .setup_logging = false});
    app.root().add_command("serve", "serve").on_run([] {});

    // each transition drives its hook, in the state it entered
    ASSERT_EQ(run_app(app, {"serve"}), 0);
    EXPECT_EQ(app.hooks,
              (std::vector<std::string> {"boot:loading",
                                         "configure:configured",
                                         "run:running",
                                         "terminate:stopping",
                                         "halt:stopped"}));

    // help halts without configuring or running
    HookApp help_app({.name = "hooks", .setup_logging = false});
    testing::internal::CaptureStdout();
    EXPECT_EQ(run_app(help_app, {"help"}), 0);
    testing::internal::GetCapturedStdout();
    EXPECT_EQ(help_app.hooks, (std::vector<std::string> {"boot:loading", "halt:stopped"}));

    // a parse error fails the boot
    HookApp fail_app({.name = "hooks", .setup_logging = false});
    EXPECT_NE(run_app(fail_app, {"--nope"}), EXIT_SUCCESS);
    EXPECT_EQ(fail_app.hooks, (std::vector<std::string> {"boot:loading", "fail:error"}));
}

TEST_F(TestCliApp, test_help_run_and_version)
{
    CliApp app({.name = "versioned", .description = "a versioned app", .version = "1.2.3", .setup_logging = false});

    testing::internal::CaptureStdout();
    EXPECT_EQ(run_app(app, {"--version"}), 0);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("1.2.3"), std::string::npos);

    CliApp app2({.name = "versioned", .description = "a versioned app", .version = "1.2.3", .setup_logging = false});
    testing::internal::CaptureStdout();
    EXPECT_EQ(run_app(app2, {"help"}), 0);
    out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("a versioned app"), std::string::npos);

    // a second boot is refused
    EXPECT_EQ(run_app(app2, {"help"}), EXIT_FAILURE);
}

TEST_F(TestCliApp, test_log_level)
{
    TestLogApp app({.name = "logs", .setup_logging = true});
    app.root().add_command("emit", "emit").on_run([] {
        SIHD_LOG(debug, "DEBUGMSG");
        SIHD_LOG(info, "INFOMSG");
    });

    testing::internal::CaptureStdout();
    run_app(app, {"--log-level", "error", "emit"});
    std::string error_level_out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(error_level_out.find("DEBUGMSG"), std::string::npos);
    EXPECT_EQ(error_level_out.find("INFOMSG"), std::string::npos);

    TestLogApp app2({.name = "logs", .setup_logging = true});
    app2.root().add_command("emit", "emit").on_run([] { SIHD_LOG(debug, "DEBUGMSG"); });
    testing::internal::CaptureStdout();
    run_app(app2, {"--log-level", "debug", "emit"});
    std::string debug_level_out = testing::internal::GetCapturedStdout();
    EXPECT_NE(debug_level_out.find("DEBUGMSG"), std::string::npos);
}

TEST_F(TestCliApp, test_log_level_unknown_warns)
{
    CliApp app({.name = "badlog", .setup_logging = true});
    app.root().add_command("emit", "emit").on_run([] {});

    testing::internal::CaptureStderr();
    run_app(app, {"--log-level", "nope", "emit"});
    std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("unknown log level"), std::string::npos);
}

TEST_F(TestCliApp, test_logging_console_colors)
{
    unset_env("NO_COLOR");
    set_env("CLICOLOR_FORCE", "1");
    {
        // forced colors even without a tty
        LogSpyApp app({.name = "colors", .setup_logging = false});
        app.install_logging();
        LoggerConsole *console = dynamic_cast<LoggerConsole *>(app.logger());
        ASSERT_NE(console, nullptr);
        EXPECT_TRUE(console->colors());
        EXPECT_TRUE(LoggerManager::get()->has_logger(console));
    }
    // NO_COLOR wins: the dated stream logger takes over
    set_env("NO_COLOR", "1");
    {
        LogSpyApp app({.name = "nocolor", .setup_logging = false});
        app.install_logging();
        EXPECT_NE(dynamic_cast<LoggerStream *>(app.logger()), nullptr);
    }
    unset_env("NO_COLOR");
    unset_env("CLICOLOR_FORCE");
}

TEST_F(TestCliApp, test_logging_console_opt_out)
{
    // opted out: nothing is installed at boot
    LogSpyApp opted_out({.name = "optout", .default_logger = false});
    opted_out.root().add_command("run", "run").on_run([] {});
    EXPECT_EQ(run_app(opted_out, {"run"}), 0);
    EXPECT_EQ(opted_out.logger(), nullptr);

    LogSpyApp app({.name = "uninstall", .setup_logging = false});
    app.install_logging();
    ALogger *logger = app.logger();
    ASSERT_NE(logger, nullptr);
    EXPECT_TRUE(LoggerManager::get()->has_logger(logger));
    app.uninstall_logging();
    EXPECT_FALSE(LoggerManager::get()->has_logger(logger));
    EXPECT_EQ(app.logger(), nullptr);
    // uninstalling twice is a no-op
    app.uninstall_logging();
}

TEST_F(TestCliApp, test_logging_conf)
{
    LogSpyApp app({.name = "conflog"});
    app.set_conf_loader([]() { return std::string(R"({"sihd": {"logging": {"console": false, "level": "debug"}}})"); });
    app.root().add_command("run", "run").on_run([] {});
    // the conf opts out of the console logger
    EXPECT_EQ(run_app(app, {"run"}), 0);
    EXPECT_EQ(app.logger(), nullptr);

    // the reload brings it back
    app.set_conf_loader([]() { return std::string(R"({"sihd": {"logging": {"console": true}}})"); });
    EXPECT_TRUE(app.reload());
    ASSERT_NE(app.logger(), nullptr);
    EXPECT_TRUE(LoggerManager::get()->has_logger(app.logger()));
}

} // namespace test
