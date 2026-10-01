#include <cstdio>

#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::ssh;

int main(int argc, char **argv)
{
    App app({
        .name = "ssh_cmd_demo",
        .description = "Execute a command in a ssh host",
    });

    std::string user;
    std::string password;
    std::string host;
    std::string cmd;
    int port = 22;
    app.root().bind("user", user, "User for the ssh connexion", "u");
    app.root().bind("password", password, "Password for the ssh connexion", "p");
    app.root().bind("host", host, "Host for the ssh connexion", "H");
    app.root().bind("port", port, "Port for the ssh connexion");
    app.root().bind("cmd", cmd, "Command to execute in ssh host", "c");

    app.root().on_run([&] {
        if (user.empty() || host.empty() || cmd.empty())
        {
            SIHD_LOG(error, "Missing --user, --host or --cmd");
            app.exit(EXIT_FAILURE);
        }

        SshSession session;
        if (SIHD_UNEXPECTED_LOG(session.fast_connect({.user = user, .host = host, .port = port})))
            app.exit(EXIT_FAILURE);

        SshSession::AuthState auth_state {-1};
        if (password.empty() == false)
            auth_state = session.auth_password(password);
        else
            auth_state = session.auth_interactive_keyboard();

        SIHD_LOG(info, "Auth status: {}", auth_state.str());
        if (auth_state.success() == false)
            app.exit(EXIT_FAILURE);

        std::string stdout_str;
        std::string stderr_str;
        sihd::util::Handler<std::string_view, bool> test_output_handler(
            [&stdout_str, &stderr_str](std::string_view buf, bool is_stderr) {
                if (is_stderr)
                    stderr_str += buf;
                else
                    stdout_str += buf;
            });

        SshCommand ssh_cmd = session.make_command();
        ssh_cmd.output_handler = &test_output_handler;
        SIHD_UNEXPECTED_LOG(ssh_cmd.execute(cmd));
        SIHD_UNEXPECTED_LOG(ssh_cmd.wait());

        if (stdout_str.empty() == false)
            fmt::print("{}", stdout_str);

        if (stderr_str.empty() == false)
            fmt::print(stderr, "{}", stderr_str);

        app.exit(ssh_cmd.exit_status());
    });

    return app.run(argc, argv);
}
