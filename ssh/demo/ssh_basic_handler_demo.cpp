/**
 * @file ssh_basic_handler_demo.cpp
 * @brief Demo showing a complete SSH server with exec, shell, and SFTP support
 *
 * This demo demonstrates:
 * - User authentication (password-based)
 * - Command execution via SSH (exec channel)
 * - Interactive shell with real PTY
 * - SFTP file transfer subsystem
 *
 * Usage:
 *   ./ssh_basic_handler_demo -k /path/to/host_key [-p port] [-d sftp_dir]
 *
 * Test commands:
 *   ssh -p 2222 demo@localhost "echo hello"        # exec command
 *   ssh -p 2222 demo@localhost                     # interactive shell
 *   sftp -P 2222 demo@localhost                    # SFTP session
 */

#include <CLI/CLI.hpp>
#include <signal.h>

#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/ssh/SshSubsystemPty.hpp>
#include <sihd/ssh/SshSubsystemSftp.hpp>

SIHD_NEW_LOGGER("ssh-demo");

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::ssh;

int main(int argc, char **argv)
{
    std::string key_path;
    std::string sftp_root = "/tmp";
    int port = 2222;
    bool verbose = false;

    CLI::App app {"SSH Server Demo - complete SSH/SFTP server"};
    app.add_option("-k,--key", key_path, "Host key file path")->required();
    app.add_option("-p,--port", port, "Port to listen on")->default_val("2222");
    app.add_option("-d,--dir", sftp_root, "SFTP root directory")->default_val("/tmp");
    app.add_flag("-v,--verbose", verbose, "Verbose logging");

    CLI11_PARSE(app, argc, argv);

    if (term::is_interactive())
        LoggerManager::console();
    else
        LoggerManager::stream();

    if (!fs::exists(key_path))
    {
        SIHD_LOG(error, "Host key not found: {}", key_path);
        return EXIT_FAILURE;
    }

    if (!fs::is_dir(sftp_root))
    {
        SIHD_LOG(error, "SFTP root directory not found: {}", sftp_root);
        return EXIT_FAILURE;
    }

    SshServer server("ssh-server");
    BasicSshServerHandler handler;

    // ========================================
    // 1. AUTHENTICATION
    // ========================================

    handler.add_allowed_user("demo", "demo");
    handler.add_allowed_user("test", "test");

    SIHD_LOG(info, "Configured users: demo/demo, test/test");

    // ========================================
    // 2. EXEC HANDLER (ssh user@host "command")
    // ========================================

    handler.set_exec_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           std::string_view command,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const struct winsize & winsize) -> ISshSubsystemHandler * {
            SIHD_LOG(info, "Exec command: {}", command);
            // Use shell mode to execute commands (like bash -c "command")
            auto *exec = new SshSubsystemExec(command);
            return exec;
        });

    // ========================================
    // 3. SHELL HANDLER (interactive ssh session)
    // ========================================

    handler.set_shell_handler_callback([]([[maybe_unused]] SshSession *session,
                                          [[maybe_unused]] SshChannel *channel,
                                          bool has_pty,
                                          const struct winsize & winsize) -> ISshSubsystemHandler * {
        if (!has_pty)
        {
            SIHD_LOG(warning, "Shell requested without PTY - rejecting");
            return nullptr;
        }

        if (!SshSubsystemPty::is_supported())
        {
            SIHD_LOG(error, "PTY not supported on this platform");
            return nullptr;
        }

        SIHD_LOG(info, "Opening interactive shell (pty {}x{})", winsize.ws_col, winsize.ws_row);

        auto *pty = new SshSubsystemPty();
        // Use /bin/bash for interactive shell
        pty->set_shell("/bin/bash");
        pty->set_args({"-i"}); // interactive mode
        return pty;
    });

    // ========================================
    // 4. SFTP SUBSYSTEM
    // ========================================

    std::string sftp_root_copy = sftp_root;
    handler.set_subsystem_handler_callback(
        [sftp_root_copy]([[maybe_unused]] SshSession *session,
                         [[maybe_unused]] SshChannel *channel,
                         std::string_view subsystem,
                         [[maybe_unused]] bool has_pty,
                         [[maybe_unused]] const struct winsize & winsize) -> ISshSubsystemHandler * {
            if (subsystem == "sftp")
            {
                SIHD_LOG(info, "SFTP session started (root: {})", sftp_root_copy);
                auto *sftp = new SshSubsystemSftp();
                sftp->set_root_path(sftp_root_copy);
                return sftp;
            }
            SIHD_LOG(warning, "Unknown subsystem: {}", subsystem);
            return nullptr;
        });

    // ========================================
    // SERVER CONFIGURATION
    // ========================================

    if (verbose)
        server.set_verbosity(3);

    if (!server.set_port(port))
    {
        SIHD_LOG(error, "Failed to set port {}", port);
        return EXIT_FAILURE;
    }

    if (!server.set_rsa_key(key_path))
    {
        SIHD_LOG(error, "Failed to load host key: {}", key_path);
        return EXIT_FAILURE;
    }

    server.set_service_wait_stop(true);
    server.set_server_handler(&handler);

    // ========================================
    // START SERVER
    // ========================================

    SIHD_LOG(info, "========================================");
    SIHD_LOG(info, "SSH Server running on port {}", port);
    SIHD_LOG(info, "SFTP root: {}", sftp_root);
    SIHD_LOG(info, "========================================");
    SIHD_LOG(info, "Test with:");
    SIHD_LOG(info, "  ssh -p {} demo@localhost \"echo hello\"", port);
    SIHD_LOG(info, "  ssh -p {} demo@localhost", port);
    SIHD_LOG(info, "  sftp -P {} demo@localhost", port);
    SIHD_LOG(info, "========================================");
    SIHD_LOG(info, "Press Ctrl-C to stop");

    // Stop server on SIGINT/SIGTERM using SigWatcher
    SigWatcher sig_watcher({SIGINT, SIGTERM}, [&server](int sig) {
        SIHD_LOG(info, "Signal {} received, stopping server...", sig);
        server.stop();
    });

    server.start();

    // Print statistics
    SIHD_LOG(info, "Server stopped. Statistics:");
    SIHD_LOG(info,
             "  Auth successes: {} password, {} pubkey",
             handler.counters().auth_password_success,
             handler.counters().auth_pubkey_success);
    SIHD_LOG(info,
             "  Auth failures:  {} password, {} pubkey",
             handler.counters().auth_password_fail,
             handler.counters().auth_pubkey_fail);
    SIHD_LOG(info,
             "  Sessions:       {} opened, {} closed",
             handler.counters().sessions_opened,
             handler.counters().sessions_closed);
    SIHD_LOG(info, "  Channels:       {} opened", handler.counters().channels_opened);

    return EXIT_SUCCESS;
}
