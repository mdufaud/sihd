#include <cxxopts.hpp>

#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::ssh;

int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "Testing utility for module util");
    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("u,user", "User for the ssh connexion", cxxopts::value<std::string>())
        ("p,password", "Password for the ssh connexion", cxxopts::value<std::string>())
        ("H,host", "Host for the ssh connexion", cxxopts::value<std::string>())
        ("c,cmd", "Command to execute in ssh host", cxxopts::value<std::string>());
    // clang-format on

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print("{}\n", options.help());
        return 0;
    }

    if (term::is_interactive() && os::is_unix && !os::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::basic();

    SshSession session;
    const std::string user = result["user"].as<std::string>();
    const std::string host = result["host"].as<std::string>();
    const std::string cmd = result["cmd"].as<std::string>();

    if (!session.fast_connect(user, host))
        return EXIT_FAILURE;

    SshSession::AuthState auth_state {-1};
    if (result.count("password"))
    {
        const std::string password = result["password"].as<std::string>();
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
        fmt::print(stdout_str);

    if (!stderr_str.empty())
        fmt::print(stderr, stderr_str);

    return ssh_cmd.exit_status();
}