#ifndef __SIHD_SSH_TEST_HELPERS_HPP__
#define __SIHD_SSH_TEST_HELPERS_HPP__

#include "sihd/util/macro.hpp"
#include <chrono>
#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/ssh/SshSubsystemSftp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <thread>

#include <functional>
#include <memory>

namespace test
{

using namespace sihd;
using namespace sihd::ssh;

SIHD_LOGGER;

// Path to test keys in resources/
inline constexpr const char *HOST_KEY_PATH = "test/resources/test_host_key_pem";
inline constexpr const char *CLIENT_KEY_PATH = "test/resources/test_client_key";
inline constexpr const char *CLIENT_PUBKEY_PATH = "test/resources/test_client_key.pub";

// RAII wrapper for a test SSH server
struct SshServerHelper
{
        SshServer server;
        BasicSshServerHandler handler;
        util::Worker worker;
        int port = 0;

        SshServerHelper(std::string_view name = "test-ssh-server"): server(std::string(name)) {}

        ~SshServerHelper() { stop(); }

        // Non-copyable
        SshServerHelper(const SshServerHelper &) = delete;
        SshServerHelper & operator=(const SshServerHelper &) = delete;

        // Enable SFTP support with a root directory
        void enable_sftp(std::string_view root_dir = "/")
        {
            std::string root(root_dir);
            handler.set_subsystem_handler_callback(
                [root]([[maybe_unused]] SshSession *session,
                       [[maybe_unused]] SshChannel *channel,
                       std::string_view subsystem,
                       [[maybe_unused]] bool has_pty,
                       [[maybe_unused]] const struct winsize & winsize) -> ISshSubsystemHandler * {
                    if (subsystem == "sftp")
                    {
                        auto *sftp_handler = new SshSubsystemSftp();
                        sftp_handler->set_root_path(root);
                        return sftp_handler;
                    }
                    return nullptr;
                });
        }

        bool start(const char *user = "testuser", const char *password = "testpass")
        {
            handler.add_allowed_user(user, password);

            // Set default exec handler that uses shell to run commands
            handler.set_exec_handler_callback(
                []([[maybe_unused]] SshSession *session,
                   [[maybe_unused]] SshChannel *channel,
                   std::string_view command,
                   [[maybe_unused]] bool has_pty,
                   [[maybe_unused]] const struct winsize & winsize) -> ISshSubsystemHandler * {
                    auto *exec = new SshSubsystemExec(command);
                    // Default: use shell mode (ParseMode::Shell is default)
                    return exec;
                });

            // Use port 0 for dynamic allocation
            if (!server.set_port(0))
                return false;
            if (!server.set_rsa_key(HOST_KEY_PATH))
                return false;

            server.set_server_handler(&handler);

            worker.set_method([this] {
                server.start();
                return true;
            });

            if (!worker.start_sync_worker("ssh-server"))
                return false;

            SIHD_DIE_FALSE(server.wait_ready(std::chrono::seconds(1)));

            port = server.get_port();
            return port > 0;
        }

        void stop()
        {
            server.stop();
            worker.stop_worker();
        }

        // Connect a client session with password authentication
        bool connect_client(SshSession & session,
                            const char *user = "testuser",
                            const char *password = "testpass")
        {
            if (!session.fast_connect(user, "127.0.0.1", port))
                return false;
            if (!session.connected())
                return false;
            auto auth = session.auth_password(password);
            return auth.success();
        }
};

// Start a test server with default configuration
inline std::unique_ptr<SshServerHelper> make_test_server(std::string_view name = "test-ssh-server",
                                                         const char *user = "testuser",
                                                         const char *password = "testpass")
{
    auto server = std::make_unique<SshServerHelper>(name);
    if (!server->start(user, password))
        return nullptr;
    return server;
}

// Start a test server with SFTP support
inline std::unique_ptr<SshServerHelper> make_test_server_with_sftp(std::string_view name = "test-ssh-server",
                                                                   std::string_view sftp_root = "/",
                                                                   const char *user = "testuser",
                                                                   const char *password = "testpass")
{
    auto server = std::make_unique<SshServerHelper>(name);
    server->enable_sftp(sftp_root);
    if (!server->start(user, password))
        return nullptr;
    return server;
}

// Helper to create an authenticated client session
inline bool connect_to_test_server(const SshServerHelper & server,
                                   SshSession & session,
                                   const char *user = "testuser",
                                   const char *password = "testpass")
{
    if (!session.fast_connect(user, "127.0.0.1", server.port))
        return false;
    if (!session.connected())
        return false;
    auto auth = session.auth_password(password);
    return auth.success();
}

} // namespace test

#endif // __SIHD_SSH_TEST_HELPERS_HPP__
