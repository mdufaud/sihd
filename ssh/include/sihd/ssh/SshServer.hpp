#ifndef __SIHD_SSH_SSHSERVER_HPP__
#define __SIHD_SSH_SSHSERVER_HPP__

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::ssh
{

class ISshServerHandler;

class SshServer: public sihd::util::Named,
                 public sihd::util::Configurable,
                 public sihd::util::ABlockingService

{
    public:
        SshServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~SshServer();

        // Default backstop applied to each connection's blocking key exchange
        static constexpr int default_kex_timeout_sec = 10;

        // Configuration methods (call before start)
        bool set_port(int port);
        bool set_bind_address(std::string_view addr);
        bool set_rsa_key(std::string_view key_path);
        bool set_ecdsa_key(std::string_view key_path);
        bool set_authorized_keys_file(std::string_view path);
        // Pre-auth issue banner (MOTD) sent to clients before authentication
        bool set_banner(std::string_view banner);
        bool set_verbosity(int level);
        // Bound the blocking key exchange (seconds) to defuse connect-and-stall DoS
        bool set_kex_timeout(int seconds);

        // Handler
        void set_server_handler(ISshServerHandler *handler);
        ISshServerHandler *server_handler() const { return _server_handler_ptr; }

        // Get the actual bound port (useful when port 0 is used for dynamic allocation)
        int get_port() const;

        // Check if server should stop (used by event loop)
        bool should_stop() const { return _stop.load(); }

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;

        ISshServerHandler *_server_handler_ptr;
        std::atomic<bool> _stop;
};

} // namespace sihd::ssh

#endif
