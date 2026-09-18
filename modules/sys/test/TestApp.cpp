#include <cstdlib>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
namespace fs = sihd::sys::fs;
namespace signal = sihd::sys::signal;
namespace os = sihd::sys::os;

static int run_app(sihd::sys::App & app, const std::vector<std::string> & args)
{
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>("prog"));
    for (const std::string & arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    return app.run((int)argv.size(), argv.data());
}

static std::string tmp_path(std::string_view suffix)
{
    return fmt::format("/tmp/sihd_app_test_{}_{}", os::pid(), suffix);
}

class TestService: public AService
{
    public:
        bool is_running() const override { return _running; }

    protected:
        bool do_start() override
        {
            started = true;
            _running = true;
            return true;
        }

        bool do_stop() override
        {
            stopped = true;
            _running = false;
            return true;
        }

    public:
        bool started = false;
        bool stopped = false;

    private:
        bool _running = false;
};

class TestApp: public ::testing::Test
{
    protected:
        TestApp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestApp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

// keeps the App logging setup, exposing what install_logging installed
class LogCheckApp: public sihd::sys::App
{
    public:
        using App::App;

        using CliApp::logger;
};

TEST_F(TestApp, test_conf_file)
{
    std::string path = tmp_path("conf.json");
    ASSERT_TRUE(fs::write(path, R"({"serve": {"host": "confhost", "port": 1234}})"));

    struct
    {
            std::string host = "localhost";
            int port = 80;
    } opts;
    sihd::sys::App app({.name = "conf", .setup_logging = false});
    std::string result;
    Command & serve = app.root().add_command("serve", "serve");
    serve.bind("host", opts.host, "");
    serve.bind("port", opts.port, "");
    serve.on_run([&] { result = fmt::format("{}:{}", opts.host, opts.port); });

    ASSERT_EQ(run_app(app, {"--conf", path, "serve"}), 0);
    EXPECT_EQ(result, "confhost:1234");
    fs::remove_file(path);
}

TEST_F(TestApp, test_conf_file_missing)
{
    sihd::sys::App app({.name = "missing", .setup_logging = false});
    bool ran = false;
    app.root().add_command("run", "run").on_run([&] { ran = true; });

    ASSERT_EQ(run_app(app, {"--conf", "/nonexistent/sihd/conf.json", "run"}), EXIT_FAILURE);
    EXPECT_EQ(app.state(), CliApp::State::error);
    EXPECT_FALSE(ran);
}

#if defined(SIGHUP) && defined(SIGUSR1) && defined(SIGPIPE)
TEST_F(TestApp, test_golden_conf)
{
    // the golden sample documents every conf key and must keep applying as a whole
    const std::string path = "test/resources/golden_app_conf.json";
    const std::string log_path = "/tmp/sihd_app.log";
    fs::remove_file(log_path);

    struct
    {
            std::string host = "localhost";
            int port = 80;
    } opts;
    int reloads = 0;
    int signaled = 0;
    // the file logger flushes on destruction
    {
        sihd::sys::App app({.name = "golden", .setup_logging = true});
        app.on_reload([&](void) { reloads++; });
        app.on_signal([&](int sig) {
            if (sig == SIGUSR1)
                signaled++;
        });
        Command & serve = app.root().add_command("serve", "serve");
        serve.bind("host", opts.host, "");
        serve.bind("port", opts.port, "");
        serve.on_run([&] {
            signal::kill(os::pid(), SIGUSR1);
            signal::kill(os::pid(), SIGPIPE);
            signal::kill(os::pid(), SIGHUP);
            app.wait_for_termination(Duration(time::milli(500)));
            SIHD_LOG(info, "GOLDENMSG");
            if (signaled == 0 || reloads == 0 || app.should_stop())
                app.exit(EXIT_FAILURE);
        });

        ASSERT_EQ(run_app(app, {"--conf", path, "serve"}), 0);
        EXPECT_EQ(opts.host, "0.0.0.0");
        EXPECT_EQ(opts.port, 8080);
        EXPECT_EQ(app.daemon().working_dir(), "/tmp");
    }
    EXPECT_EQ(reloads, 1);
    EXPECT_EQ(signaled, 1);
    std::optional<std::string> content = fs::read_all(log_path);
    ASSERT_TRUE(content.has_value());
    EXPECT_NE(content->find("GOLDENMSG"), std::string::npos);
    fs::remove_file(log_path);
}
#endif

#if defined(SIGUSR1)
TEST_F(TestApp, test_signal_unknown_conf)
{
    std::string path = tmp_path("badsig.json");
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"ignore": ["NOPE"]}}})"));

    sihd::sys::App app({.name = "badsig", .setup_logging = false});
    bool ran = false;
    app.root().add_command("run", "run").on_run([&] { ran = true; });

    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), EXIT_FAILURE);
    EXPECT_FALSE(ran);
    fs::remove_file(path);
}
#endif

TEST_F(TestApp, test_signal_stop)
{
    // windows kill terminates the process without running handlers
    if constexpr (sihd::util::build::is_windows)
        GTEST_SKIP() << "not supported on windows";

    sihd::sys::App app({.name = "stop", .setup_logging = false});
    app.root().add_command("run", "run").on_run([&] {
        signal::kill(os::pid(), SIGINT);
        app.wait_for_termination(Duration(time::milli(2000)));
        if (app.should_stop() == false)
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"run"}), 0);
    EXPECT_TRUE(app.should_stop());
    EXPECT_EQ(app.state(), CliApp::State::stopped);
}

#if defined(SIGHUP)
TEST_F(TestApp, test_signal_reload)
{
    std::string path = tmp_path("reload.json");
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"reload": ["HUP"]}}, "serve": {"port": 9}})"));

    struct
    {
            int port = 80;
    } opts;
    sihd::sys::App app({.name = "reload", .setup_logging = false});
    int reloads = 0;
    app.on_reload([&](void) { reloads++; });
    app.root().add_command("serve", "serve").bind("port", opts.port, "").on_run([&] {
        signal::kill(os::pid(), SIGHUP);
        app.wait_for_termination(Duration(time::milli(500)));
        if (reloads == 0 || app.should_stop())
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"--conf", path, "serve"}), 0);
    EXPECT_EQ(reloads, 1);
    fs::remove_file(path);
}

# if defined(SIGUSR2)
TEST_F(TestApp, test_signal_reload_custom)
{
    std::string path = tmp_path("customreload.json");
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"reload": ["USR2"]}}})"));

    sihd::sys::App app({.name = "customreload", .setup_logging = false});
    int reloads = 0;
    app.on_reload([&](void) { reloads++; });
    app.root().add_command("run", "run").on_run([&] {
        signal::kill(os::pid(), SIGUSR2);
        app.wait_for_termination(Duration(time::milli(300)));
        if (reloads == 0 || app.should_stop())
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), 0);
    EXPECT_EQ(reloads, 1);
    fs::remove_file(path);
}
# endif

#endif

#if defined(SIGUSR1)
TEST_F(TestApp, test_signal_added_on_reload)
{
    std::string path = tmp_path("sigreload.json");
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"reload": ["HUP"]}}})"));

    sihd::sys::App app({.name = "sigreload", .setup_logging = false});
    app.root().add_command("run", "run").on_run([&] {
        ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"reload": ["HUP"], "ignore": ["USR1"]}}})"));
        ASSERT_TRUE(app.reload());
        signal::kill(os::pid(), SIGUSR1);
        app.wait_for_termination(Duration(time::milli(300)));
        if (app.should_stop())
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), 0);
    fs::remove_file(path);
}
#endif

#if defined(SIGUSR1)
TEST_F(TestApp, test_signal_ignore)
{
    std::string path = tmp_path("ignore.json");
    // the conf accepts raw signal numbers too
    ASSERT_TRUE(fs::write(path, fmt::format(R"({{"sihd": {{"signals": {{"ignore": [{}]}}}}}})", SIGUSR1)));

    sihd::sys::App app({.name = "ignore", .setup_logging = false});
    app.root().add_command("run", "run").on_run([&] {
        signal::kill(os::pid(), SIGUSR1);
        app.wait_for_termination(Duration(time::milli(300)));
        if (app.should_stop())
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), 0);
    fs::remove_file(path);
}
#endif

#if defined(SIGUSR1)
TEST_F(TestApp, test_signal_float_conf_rejected)
{
    std::string path = tmp_path("floatsig.json");
    // 2.5 must not silently become signal 2
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"ignore": [2.5]}}})"));

    sihd::sys::App app({.name = "floatsig", .setup_logging = false});
    bool ran = false;
    app.root().add_command("run", "run").on_run([&] { ran = true; });

    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), EXIT_FAILURE);
    EXPECT_FALSE(ran);
    fs::remove_file(path);
}
#endif

#if defined(SIGUSR1)
TEST_F(TestApp, test_signal_callback)
{
    std::string path = tmp_path("callback.json");
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"signals": {"callback": ["USR2"]}}})"));

    sihd::sys::App app({.name = "callback", .setup_logging = false});
    int signaled = 0;
    app.on_signal([&](int sig) {
        if (sig == SIGUSR2)
            signaled++;
    });
    app.root().add_command("run", "run").on_run([&] {
        signal::kill(os::pid(), SIGUSR2);
        app.wait_for_termination(Duration(time::milli(300)));
        if (signaled == 0)
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), 0);
    fs::remove_file(path);
}

#endif

TEST_F(TestApp, test_daemon_keys)
{
    std::string path = tmp_path("daemon.json");
    ASSERT_TRUE(fs::write(path, R"({"sihd": {"daemon": {"working_dir": "/tmp"}}})"));

    sihd::sys::App app({.name = "daemon", .setup_logging = false});
    app.root().add_command("run", "run").on_run([] {});

    // the daemon keys configure the Daemon without forking in tests
    ASSERT_EQ(run_app(app, {"--conf", path, "run"}), 0);
    if constexpr (sihd::util::build::is_windows == false)
    {
        EXPECT_TRUE(app.daemon().supported);
    }
    EXPECT_EQ(app.daemon().working_dir(), "/tmp");
    fs::remove_file(path);
}

TEST_F(TestApp, test_service_loop)
{
    sihd::sys::App app({.name = "loop", .setup_logging = false});
    TestService service;
    app.root().add_command("run", "run").on_run([&] {
        EXPECT_TRUE(service.start());
        app.wait_for_termination(Duration(time::milli(300)));
        EXPECT_TRUE(service.stop());
        if (service.started == false || service.stopped == false)
            app.exit(EXIT_FAILURE);
    });

    ASSERT_EQ(run_app(app, {"run"}), 0);
}

TEST_F(TestApp, test_log_file)
{
    std::string log_path = tmp_path("app.log");
    std::string path = tmp_path("logconf.json");
    ASSERT_TRUE(fs::write(path, fmt::format(R"({{"sihd": {{"logging": {{"file": "{}"}}}}}})", log_path)));

    // the file logger flushes on destruction
    {
        sihd::sys::App app({.name = "logfile", .setup_logging = true});
        app.root().add_command("emit", "emit").on_run([] { SIHD_LOG(info, "FILEMSG"); });

        ASSERT_EQ(run_app(app, {"--conf", path, "emit"}), 0);
    }
    std::optional<std::string> content = fs::read_all(log_path);
    ASSERT_TRUE(content.has_value());
    EXPECT_NE(content->find("FILEMSG"), std::string::npos);
    fs::remove_file(path);
    fs::remove_file(log_path);
}

TEST_F(TestApp, test_log_file_level)
{
    std::string log_path = tmp_path("filevel.log");
    std::string path = tmp_path("filevel.json");
    ASSERT_TRUE(
        fs::write(path, fmt::format(R"({{"sihd": {{"logging": {{"level": "info", "file": "{}"}}}}}})", log_path)));

    // the file logger flushes on destruction
    {
        sihd::sys::App app({.name = "filevel", .setup_logging = true});
        app.root().add_command("emit", "emit").on_run([] {
            SIHD_LOG(debug, "DEBUGMSG");
            SIHD_LOG(info, "INFOMSG");
        });

        ASSERT_EQ(run_app(app, {"--conf", path, "emit"}), 0);
    }
    std::optional<std::string> content = fs::read_all(log_path);
    ASSERT_TRUE(content.has_value());
    // the level filter applies to the file logger
    EXPECT_NE(content->find("INFOMSG"), std::string::npos);
    EXPECT_EQ(content->find("DEBUGMSG"), std::string::npos);
    fs::remove_file(path);
    fs::remove_file(log_path);
}

TEST_F(TestApp, test_log_file_reload)
{
    std::string log_path1 = tmp_path("reload1.log");
    std::string log_path2 = tmp_path("reload2.log");
    std::string path = tmp_path("logreload.json");
    ASSERT_TRUE(fs::write(path, fmt::format(R"({{"sihd": {{"logging": {{"file": "{}"}}}}}})", log_path1)));

    // the file loggers flush on destruction
    {
        sihd::sys::App app({.name = "logreload", .setup_logging = true});
        std::string msg = "FIRSTMSG";
        app.root().add_command("emit", "emit").on_run([&] { SIHD_LOG(info, "{}", msg); });

        ASSERT_EQ(run_app(app, {"--conf", path, "emit"}), 0);
        ASSERT_TRUE(fs::write(path, fmt::format(R"({{"sihd": {{"logging": {{"file": "{}"}}}}}})", log_path2)));
        msg = "SECONDMSG";
        ASSERT_TRUE(app.reload());
        EXPECT_EQ(app.evaluate(std::vector<std::string> {"emit"}), 0);
    }
    std::optional<std::string> first = fs::read_all(log_path1);
    ASSERT_TRUE(first.has_value());
    EXPECT_NE(first->find("FIRSTMSG"), std::string::npos);
    EXPECT_EQ(first->find("SECONDMSG"), std::string::npos);
    std::optional<std::string> second = fs::read_all(log_path2);
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(second->find("SECONDMSG"), std::string::npos);
    fs::remove_file(path);
    fs::remove_file(log_path1);
    fs::remove_file(log_path2);
}

TEST_F(TestApp, test_log_console_opt_out)
{
    std::string log_path = tmp_path("noconsole.log");
    std::string path = tmp_path("noconsole.json");
    ASSERT_TRUE(
        fs::write(path, fmt::format(R"({{"sihd": {{"logging": {{"console": false, "file": "{}"}}}}}})", log_path)));

    // the file logger flushes on destruction
    {
        LogCheckApp app({.name = "noconsole"});
        app.root().add_command("emit", "emit").on_run([] { SIHD_LOG(info, "NOCONSOLEMSG"); });

        ASSERT_EQ(run_app(app, {"--conf", path, "emit"}), 0);
        // the conf opted out of the console, the file logger stayed
        EXPECT_EQ(app.logger(), nullptr);
    }
    std::optional<std::string> content = fs::read_all(log_path);
    ASSERT_TRUE(content.has_value());
    EXPECT_NE(content->find("NOCONSOLEMSG"), std::string::npos);
    fs::remove_file(path);
    fs::remove_file(log_path);
}

TEST_F(TestApp, test_log_console_option)
{
    LogCheckApp app({.name = "noconsoleopt", .default_logger = false});
    bool ran = false;
    app.root().add_command("run", "run").on_run([&] { ran = true; });

    ASSERT_EQ(run_app(app, {"run"}), 0);
    EXPECT_TRUE(ran);
    EXPECT_EQ(app.logger(), nullptr);
}

} // namespace test
