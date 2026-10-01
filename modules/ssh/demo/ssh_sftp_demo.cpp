#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::ssh;

void print_dir(Sftp & sftp, std::string_view path)
{
    fmt::print("Listing directories in '{}':\n", path);
    std::vector<sihd::ssh::SftpAttribute> list;
    if (sftp.list_dir(path, list).has_value())
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
    App app({
        .name = "ssh_sftp_demo",
        .description = "List directories through sftp",
    });

    std::string user;
    std::string password;
    std::string path;
    std::string host;
    int port = 22;
    app.root().bind("user", user, "User for the ssh connexion", "u");
    app.root().bind("password", password, "Password for the ssh connexion", "p");
    app.root().bind("path", path, "Path for the sftp list dir", "P");
    app.root().bind("host", host, "Host for the ssh connexion", "H");
    app.root().bind("port", port, "Port for the ssh connexion");

    app.root().on_run([&] {
        if (user.empty() || host.empty() || path.empty())
        {
            SIHD_LOG(error, "Missing --user, --host or --path");
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

        Sftp sftp = session.make_sftp();
        if (SIHD_UNEXPECTED_LOG(sftp.open()))
            app.exit(EXIT_FAILURE);
        print_extensions(sftp);
        print_dir(sftp, path);
    });

    return app.run(argc, argv);
}
