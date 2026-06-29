#ifndef __SIHD_CRYPTO_TLSCONTEXT_HPP__
#define __SIHD_CRYPTO_TLSCONTEXT_HPP__

#include <string_view>

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

        bool init(bool server_mode);

        bool set_certificate(const Certificate & cert);
        bool set_private_key(const PrivateKey & key);
        bool load_ca_cert(std::string_view path);
        void set_verify_peer(bool verify);

        void clear();

        void *native() const { return _handle; }

    private:
        void *_handle;
};

} // namespace sihd::crypto

#endif
