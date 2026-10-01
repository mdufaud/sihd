#ifndef __SIHD_CRYPTO_PRIVATEKEY_HPP__
#define __SIHD_CRYPTO_PRIVATEKEY_HPP__

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Error.hpp>

namespace sihd::crypto
{

class PrivateKey
{
    public:
        PrivateKey();
        ~PrivateKey();

        PrivateKey(const PrivateKey & other);
        PrivateKey & operator=(const PrivateKey & other);
        PrivateKey(PrivateKey && other) noexcept;
        PrivateKey & operator=(PrivateKey && other) noexcept;

        operator bool() const { return _handle != nullptr; }

        std::expected<void, sihd::util::Error> generate_rsa(int bits = 2048);
        std::expected<void, sihd::util::Error> generate_ec(std::string_view curve_name = "prime256v1");

        std::expected<void, sihd::util::Error> load_pem(std::string_view path);
        std::expected<void, sihd::util::Error> save_pem(std::string_view path) const;

        std::expected<void, sihd::util::Error> load_pem_string(std::string_view pem);
        std::string to_pem_string() const;

        void clear();

        void *native() const { return _handle; }

    private:
        void *_handle;
};

} // namespace sihd::crypto

#endif
