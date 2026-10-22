#ifndef __SIHD_SSH_TEST_HELPERS_HPP__
#define __SIHD_SSH_TEST_HELPERS_HPP__

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/ssh/SshSubsystemSftp.hpp>
#include <sihd/ssh/WinSize.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>

#include "sihd/util/macro.hpp"

namespace test
{

inline constexpr const char *HOST_KEY_PATH = "test/resources/test_host_key_pem";
inline constexpr const char *CLIENT_KEY_PATH = "test/resources/test_client_key";
inline constexpr const char *CLIENT_PUBKEY_PATH = "test/resources/test_client_key.pub";

struct SshServerHelper
{
        // declared before the server so it is destroyed after it
        sihd::ssh::BasicSshServerHandler handler;
        sihd::ssh::SshServer server;
        sihd::util::Worker worker;
        int port = 0;

        SshServerHelper(std::string_view name = "test-ssh-server"): server(std::string(name)) {}

        ~SshServerHelper() { stop(); }

        SshServerHelper(const SshServerHelper &) = delete;
        SshServerHelper & operator=(const SshServerHelper &) = delete;

        void enable_sftp(std::string_view root_dir = "/")
        {
            std::string root(root_dir);
            handler.set_subsystem_handler_callback(
                [root]([[maybe_unused]] sihd::ssh::SshSession *session,
                       [[maybe_unused]] sihd::ssh::SshChannel *channel,
                       std::string_view subsystem,
                       [[maybe_unused]] bool has_pty,
                       [[maybe_unused]] const sihd::ssh::WinSize & winsize) -> sihd::ssh::ISshSubsystemHandler * {
                    if (subsystem == "sftp")
                    {
                        auto *sftp_handler = new sihd::ssh::SshSubsystemSftp();
                        sftp_handler->set_root_path(root);
                        return sftp_handler;
                    }
                    return nullptr;
                });
        }

        bool start(const char *user = "testuser", const char *password = "testpass")
        {
            handler.add_allowed_user(user, password);

            handler.set_exec_handler_callback(
                []([[maybe_unused]] sihd::ssh::SshSession *session,
                   [[maybe_unused]] sihd::ssh::SshChannel *channel,
                   std::string_view command,
                   [[maybe_unused]] bool has_pty,
                   [[maybe_unused]] const sihd::ssh::WinSize & winsize) -> sihd::ssh::ISshSubsystemHandler * {
                    auto *exec = new sihd::ssh::SshSubsystemExec(command);
                    return exec;
                });

            if (!server.set_port(0).has_value())
                return false;
            if (!server.set_rsa_key(HOST_KEY_PATH).has_value())
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
};

inline std::unique_ptr<SshServerHelper> make_test_server(std::string_view name = "test-ssh-server",
                                                         const char *user = "testuser",
                                                         const char *password = "testpass")
{
    auto server = std::make_unique<SshServerHelper>(name);
    if (!server->start(user, password))
        return nullptr;
    return server;
}

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

bool connect_to_test_server(const SshServerHelper & server,
                            sihd::ssh::SshSession & session,
                            const char *user = "testuser",
                            const char *password = "testpass");

} // namespace test

#endif // __SIHD_SSH_TEST_HELPERS_HPP__
