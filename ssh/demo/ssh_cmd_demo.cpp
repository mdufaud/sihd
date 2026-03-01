#include <CLI/CLI.hpp>

#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::ssh;

int main(int argc, char **argv)
{
    std::string user;
    std::string password;
    std::string host;
    std::string cmd;

    CLI::App app {"Testing utility for module util"};
    app.add_option("-u,--user", user, "User for the ssh connexion")->required();
    app.add_option("-p,--password", password, "Password for the ssh connexion");
    app.add_option("-H,--host", host, "Host for the ssh connexion")->required();
    app.add_option("-c,--cmd", cmd, "Command to execute in ssh host")->required();

    CLI11_PARSE(app, argc, argv);

    if (term::is_interactive() && platform::is_unix && !platform::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::stream();

    SshSession session;
    if (!session.fast_connect(user, host))
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
    ssh_cmd.execute(cmd);
    ssh_cmd.wait();

    if (!stdout_str.empty())
        fmt::print("{}", stdout_str);

    if (!stderr_str.empty())
        fmt::print(stderr, "{}", stderr_str);

    return ssh_cmd.exit_status();
}