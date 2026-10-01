#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshShell.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::ssh;

int main(int argc, char **argv)
{
    App app({
        .name = "ssh_shell_demo",
        .description = "Open an interactive ssh shell",
    });

    std::string user;
    std::string password;
    std::string host;
    int port = 22;
    app.root().bind("user", user, "User for the ssh connexion", "u");
    app.root().bind("password", password, "Password for the ssh connexion", "p");
    app.root().bind("host", host, "Host for the ssh connexion", "H");
    app.root().bind("port", port, "Port for the ssh connexion");

    app.root().on_run([&] {
        if (user.empty() || host.empty())
        {
            SIHD_LOG(error, "Missing --user or --host");
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

        SshShell shell = session.make_shell();
        if (shell.open().has_value() == false)
            app.exit(EXIT_FAILURE);
        if (shell.read_loop() == false)
            app.exit(EXIT_FAILURE);
    });

    return app.run(argc, argv);
}
