#ifndef __SIHD_CRYPTO_TLSCONTEXT_HPP__
#define __SIHD_CRYPTO_TLSCONTEXT_HPP__

#include <memory>
#include <string_view>

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::crypto
{

class PrivateKey;
class Certificate;

class TlsContext
{
    public:
        TlsContext() = default;
        ~TlsContext() = default;

        TlsContext(const TlsContext & other) = default;
        TlsContext & operator=(const TlsContext & other) = default;
        TlsContext(TlsContext && other) noexcept = default;
        TlsContext & operator=(TlsContext && other) noexcept = default;

        operator bool() const;

        // TLS 1.2 minimum, renegotiation disabled, a client verifies the server certificate against the default CAs
        sihd::util::Result<void> init(bool server_mode);

        sihd::util::Result<void> set_certificate(const Certificate & cert);
        sihd::util::Result<void> set_private_key(const PrivateKey & key);
        sihd::util::Result<void> load_ca_cert(sihd::util::ZStringView path);
        // client: verify the server certificate (on by default) - server: require a client certificate
        sihd::util::Result<void> set_verify_peer(bool verify);
        // server: scope of resumable sessions (1 to 32 bytes); unset with set_verify_peer, a resuming client fails
        sihd::util::Result<void> set_session_id_context(std::string_view context);
        // opt out of the default refusal: TLS 1.2 renegotiation has a history of CVEs and lets a peer burn server CPU
        sihd::util::Result<void> set_renegotiation(bool allow);

        void clear();

        void *native() const;

    private:
        // type-erased SSL_CTX, deleter bound at acquisition: keeps openssl types out of this header
        std::shared_ptr<void> _handle;
        bool _server_mode = false;
};

} // namespace sihd::crypto

#endif
