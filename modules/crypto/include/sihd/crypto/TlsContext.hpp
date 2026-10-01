#ifndef __SIHD_CRYPTO_TLSCONTEXT_HPP__
#define __SIHD_CRYPTO_TLSCONTEXT_HPP__

#include <expected>
#include <string_view>

#include <sihd/util/Error.hpp>

namespace sihd::crypto
{

class PrivateKey;
class Certificate;

class TlsContext
{
    public:
        TlsContext();
        ~TlsContext();

        TlsContext(const TlsContext & other);
        TlsContext & operator=(const TlsContext & other);
        TlsContext(TlsContext && other) noexcept;
        TlsContext & operator=(TlsContext && other) noexcept;

        operator bool() const { return _handle != nullptr; }

        std::expected<void, sihd::util::Error> init(bool server_mode);

        std::expected<void, sihd::util::Error> set_certificate(const Certificate & cert);
        std::expected<void, sihd::util::Error> set_private_key(const PrivateKey & key);
        std::expected<void, sihd::util::Error> load_ca_cert(std::string_view path);
        void set_verify_peer(bool verify);

        void clear();

        void *native() const { return _handle; }

    private:
        void *_handle;
};

} // namespace sihd::crypto

#endif
