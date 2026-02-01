#include <CLI/CLI.hpp>

#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::ssh;

void print_dir(Sftp & sftp, std::string_view path)
{
    fmt::print("Listing directories in '{}':\n", path);
    std::vector<sihd::ssh::SftpAttribute> list;
    if (sftp.list_dir(path, list))
    {
        container::sort(list, [](const SftpAttribute & attr1, const SftpAttribute & attr2) {
            if (str::starts_with(attr1.name(), ".") != str::starts_with(attr2.name(), "."))
                return str::starts_with(attr1.name(), ".") > str::starts_with(attr2.name(), ".");

            if (attr1.type() != attr2.type())
                return attr1.type() > attr2.type();

            return attr1.name() < attr2.name();
        });

        for (const auto & attr : list)
        {
            fmt::print("{}", attr.name());
            if (attr.is_dir())
                fmt::print("/");
            if (attr.is_file())
                fmt::print(" ({} bytes)", attr.size());
            if (attr.is_link())
                fmt::print(" -> {}", sftp.readlink(fs::combine(path, attr.name())));
            fmt::print("\n");
        }
    }
    fmt::print("\n");
}

void print_extensions(Sftp & sftp)
{
    fmt::print("Extensions:\n");
    std::vector<SftpExtension> extensions = sftp.extensions();
    for (const SftpExtension & extension : extensions)
    {
        fmt::print("  {} :: {}\n", extension.name, extension.data);
    }
    fmt::print("\n");
}

int main(int argc, char **argv)
{
    std::string user;
    std::string password;
    std::string path;
    std::string host;

    CLI::App app {"Testing utility for module util"};
    app.add_option("-u,--user", user, "User for the ssh connexion")->required();
    app.add_option("-p,--password", password, "Password for the ssh connexion");
    app.add_option("-P,--path", path, "Path for the sftp list dir")->required();
    app.add_option("-H,--host", host, "Host for the ssh connexion")->required();

    CLI11_PARSE(app, argc, argv);

    if (term::is_interactive() && os::is_unix && !os::is_emscripten)
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

    Sftp sftp = session.make_sftp();
    if (sftp.open())
    {
        print_extensions(sftp);
        print_dir(sftp, path);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}