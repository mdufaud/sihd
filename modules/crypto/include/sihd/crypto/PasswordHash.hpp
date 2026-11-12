#ifndef __SIHD_CRYPTO_PASSWORDHASH_HPP__
#define __SIHD_CRYPTO_PASSWORDHASH_HPP__

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <sihd/util/Result.hpp>

namespace sihd::crypto
{

// defaults are the OWASP minimums, pbkdf2 the sha256 one
struct Argon2Params
{
        uint32_t memory_kib = 19456;
        uint32_t iterations = 2;
        uint32_t parallelism = 1;
};

struct ScryptParams
{
        uint32_t log2_n = 17;
        uint32_t block_size = 8;
        uint32_t parallelism = 1;
};

struct Pbkdf2Params
{
        uint32_t iterations = 600000;
};

// encoded as a PHC string: $<algorithm>$<params>$<salt>$<hash>, base64 without padding
class PasswordHash
{
    public:
        enum class Algorithm
        {
            argon2id,
            scrypt,
            pbkdf2_sha256,
            pbkdf2_sha512,
        };

        using Params = std::variant<Argon2Params, ScryptParams, Pbkdf2Params>;

        // random 16 bytes salt, 32 bytes hash (pbkdf2: the digest size)
        static sihd::util::Result<PasswordHash> argon2id(std::string_view password, const Argon2Params & params = {});
        static sihd::util::Result<PasswordHash> scrypt(std::string_view password, const ScryptParams & params = {});
        static sihd::util::Result<PasswordHash> pbkdf2_sha256(std::string_view password,
                                                              const Pbkdf2Params & params = {});
        static sihd::util::Result<PasswordHash> pbkdf2_sha512(std::string_view password,
                                                              const Pbkdf2Params & params = {});

        // not_supported for an unknown algorithm or argon2 version, invalid_argument for a malformed string
        static sihd::util::Result<PasswordHash> decode(std::string_view phc);

        std::string encode() const;

        sihd::util::Result<bool> verify(std::string_view password) const;

        Algorithm algorithm() const;
        const Params & params() const;
        const std::vector<uint8_t> & salt() const;
        const std::vector<uint8_t> & hash() const;

    private:
        PasswordHash(Algorithm algorithm, Params params, std::vector<uint8_t> salt, std::vector<uint8_t> hash);

        static sihd::util::Result<PasswordHash>
            _generate(Algorithm algorithm, Params params, std::string_view password);

        Algorithm _algorithm;
        Params _params;
        std::vector<uint8_t> _salt;
        std::vector<uint8_t> _hash;
};

} // namespace sihd::crypto

#endif
