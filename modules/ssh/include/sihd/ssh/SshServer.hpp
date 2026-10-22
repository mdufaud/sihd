#ifndef __SIHD_SSH_SSHSERVER_HPP__
#define __SIHD_SSH_SSHSERVER_HPP__

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

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

        static constexpr int default_kex_timeout_sec = 10;

        sihd::util::Result<void> set_port(int port);
        sihd::util::Result<void> set_bind_address(std::string_view addr);
        sihd::util::Result<void> set_rsa_key(std::string_view key_path);
        sihd::util::Result<void> set_ecdsa_key(std::string_view key_path);
        sihd::util::Result<void> set_authorized_keys_file(std::string_view path);
        // Pre-auth issue banner (MOTD) sent to clients before authentication
        sihd::util::Result<void> set_banner(std::string_view banner);
        sihd::util::Result<void> set_verbosity(int level);
        // Bound the blocking key exchange (seconds) to defuse connect-and-stall DoS
        sihd::util::Result<void> set_kex_timeout(int seconds);

        void set_server_handler(ISshServerHandler *handler);
        ISshServerHandler *server_handler() const;

        int get_port() const;

        bool should_stop() const;

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
