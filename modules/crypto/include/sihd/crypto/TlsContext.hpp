#ifndef __SIHD_CRYPTO_TLSCONTEXT_HPP__
#define __SIHD_CRYPTO_TLSCONTEXT_HPP__

#include <memory>
#include <string_view>

#include <sihd/util/Result.hpp>

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

        operator bool() const { return _handle != nullptr; }

        sihd::util::Result<void> init(bool server_mode);

        sihd::util::Result<void> set_certificate(const Certificate & cert);
        sihd::util::Result<void> set_private_key(const PrivateKey & key);
        sihd::util::Result<void> load_ca_cert(std::string_view path);
        void set_verify_peer(bool verify);

        void clear();

        void *native() const { return _handle.get(); }

    private:
        // type-erased SSL_CTX: the deleter freeing it is bound at acquisition,
        // keeping openssl types out of this header
        std::shared_ptr<void> _handle;
};

} // namespace sihd::crypto

#endif
