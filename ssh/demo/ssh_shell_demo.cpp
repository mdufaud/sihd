#include <cxxopts.hpp>

#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshShell.hpp>
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
        ("H,host", "Host for the ssh connexion", cxxopts::value<std::string>());
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
        LoggerManager::stream();

    SshSession session;
    const std::string user = result["user"].as<std::string>();
    const std::string host = result["host"].as<std::string>();

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