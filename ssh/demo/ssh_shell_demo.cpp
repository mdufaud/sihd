#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshShell.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

#include <CLI/CLI.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::ssh;

int main(int argc, char **argv)
{
    std::string user;
    std::string password;
    std::string host;
    int port = 22;

    CLI::App app {"Testing utility for module util"};
    app.add_option("-u,--user", user, "User for the ssh connexion")->required();
    app.add_option("-p,--password", password, "Password for the ssh connexion");
    app.add_option("-H,--host", host, "Host for the ssh connexion")->required();
    app.add_option("--port", port, "Port for the ssh connexion")->default_val("22");

    CLI11_PARSE(app, argc, argv);

    if (term::is_interactive() && build::is_unix && !build::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::stream();

    SshSession session;
    if (!session.fast_connect({.user = user, .host = host, .port = port}))
        return EXIT_FAILURE;

    SshSession::AuthState auth_state {-1};
    if (!password.empty())
    {
        auth_state = session.auth_password(password);
    }
    else
    {
        auth_state = session.auth_interactive_keyboard();
    }

    SIHD_LOG(info, "Auth status: {}", auth_state.str());
    if (!auth_state.success())
        return EXIT_FAILURE;

    SshShell shell = session.make_shell();

    if (shell.open())
    {
        if (shell.read_loop())
        {
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}