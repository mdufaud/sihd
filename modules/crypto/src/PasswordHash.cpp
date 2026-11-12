#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/crypto/PasswordHash.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/kdf.h>
#include <openssl/opensslv.h>
#include <openssl/params.h>
#include <openssl/rand.h>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

using Algorithm = PasswordHash::Algorithm;

constexpr size_t salt_size = 16;
constexpr size_t hash_size = 32;
constexpr size_t sha512_size = 64;
// PHC string format minimums
constexpr size_t min_salt_size = 8;
constexpr size_t min_hash_size = 4;
// argon2 specification version 1.3
constexpr uint32_t argon2_version = 0x13;

struct AlgorithmId
{
        Algorithm algorithm;
        std::string_view id;
};

constexpr std::array<AlgorithmId, 4> algorithm_ids = {{
    {Algorithm::argon2id, "argon2id"},
    {Algorithm::scrypt, "scrypt"},
    {Algorithm::pbkdf2_sha256, "pbkdf2-sha256"},
    {Algorithm::pbkdf2_sha512, "pbkdf2-sha512"},
}};

// in the order of the params struct members
constexpr std::array<std::string_view, 3> argon2_keys = {"m=", "t=", "p="};
constexpr std::array<std::string_view, 3> scrypt_keys = {"ln=", "r=", "p="};
constexpr std::array<std::string_view, 1> pbkdf2_keys = {"i="};
constexpr std::array<std::string_view, 2> pbkdf2_length_keys = {"i=", "l="};

struct KdfFree
{
        void operator()(EVP_KDF *kdf) const { EVP_KDF_free(kdf); }
};

struct KdfCtxFree
{
        void operator()(EVP_KDF_CTX *ctx) const { EVP_KDF_CTX_free(ctx); }
};

struct ParamsFormatter
{
        std::string operator()(const Argon2Params & params) const
        {
            return fmt::format("m={},t={},p={}", params.memory_kib, params.iterations, params.parallelism);
        }

        std::string operator()(const ScryptParams & params) const
        {
            return fmt::format("ln={},r={},p={}", params.log2_n, params.block_size, params.parallelism);
        }

        std::string operator()(const Pbkdf2Params & params) const { return fmt::format("i={}", params.iterations); }
};

Result<std::vector<uint8_t>> run_kdf(const char *name, OSSL_PARAM *params, size_t size);

// takes params by value: OSSL_PARAM points to mutable values
struct Deriver
{
        Algorithm algorithm;
        std::string_view password;
        const std::vector<uint8_t> & salt;
        size_t size;

        OSSL_PARAM password_param() const
        {
            return OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                     const_cast<char *>(password.data()),
                                                     password.size());
        }

        OSSL_PARAM salt_param() const
        {
            return OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                     const_cast<uint8_t *>(salt.data()),
                                                     salt.size());
        }

        Result<std::vector<uint8_t>> operator()(Argon2Params params) const
        {
            if (params.iterations == 0 || params.parallelism == 0
                || params.memory_kib < uint64_t {8} * params.parallelism)
            {
                return std::unexpected(
                    Error(invalid_argument, "invalid argon2id params '{}'", ParamsFormatter {}(params)));
            }
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
            // lanes are computed in sequence: no openssl thread pool to set up
            uint32_t threads = 1;
            uint32_t version = argon2_version;
            std::array ossl_params = {
                password_param(),
                salt_param(),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &params.iterations),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &params.memory_kib),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &params.parallelism),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_VERSION, &version),
                OSSL_PARAM_construct_end(),
            };
            return run_kdf("ARGON2ID", ossl_params.data(), size);
#else
            return std::unexpected(Error(not_supported, "ARGON2ID needs OpenSSL 3.2 or later"));
#endif
        }

        Result<std::vector<uint8_t>> operator()(ScryptParams params) const
        {
            if (params.log2_n == 0 || params.log2_n >= 64 || params.block_size == 0 || params.parallelism == 0)
                return std::unexpected(
                    Error(invalid_argument, "invalid scrypt params '{}'", ParamsFormatter {}(params)));
            uint64_t cost = uint64_t {1} << params.log2_n;
            std::array ossl_params = {
                password_param(),
                salt_param(),
                OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_N, &cost),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_R, &params.block_size),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_P, &params.parallelism),
                OSSL_PARAM_construct_end(),
            };
            return run_kdf("SCRYPT", ossl_params.data(), size);
        }

        Result<std::vector<uint8_t>> operator()(Pbkdf2Params params) const
        {
            if (params.iterations == 0)
                return std::unexpected(
                    Error(invalid_argument, "invalid pbkdf2 params '{}'", ParamsFormatter {}(params)));
            const char *digest = algorithm == Algorithm::pbkdf2_sha512 ? "SHA512" : "SHA256";
            std::array ossl_params = {
                password_param(),
                salt_param(),
                OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &params.iterations),
                OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char *>(digest), 0),
                OSSL_PARAM_construct_end(),
            };
            return run_kdf("PBKDF2", ossl_params.data(), size);
        }
};

Result<std::vector<uint8_t>> run_kdf(const char *name, OSSL_PARAM *params, size_t size)
{
    ERR_clear_error();
    std::unique_ptr<EVP_KDF, KdfFree> kdf(EVP_KDF_fetch(nullptr, name, nullptr));
    if (kdf == nullptr)
    {
        // fetch failure queues openssl errors: clear so the next make_error is not misattributed
        ERR_clear_error();
        return std::unexpected(Error(not_supported, "{} is not supported by this OpenSSL", name));
    }

    std::unique_ptr<EVP_KDF_CTX, KdfCtxFree> ctx(EVP_KDF_CTX_new(kdf.get()));
    if (ctx == nullptr)
        return make_error("EVP_KDF_CTX_new");

    std::vector<uint8_t> out(size);
    if (EVP_KDF_derive(ctx.get(), out.data(), out.size(), params) != 1)
        return make_error("EVP_KDF_derive");
    return out;
}

std::string encode_base64(const std::vector<uint8_t> & bytes)
{
    std::string encoded = str::to_base64(bytes.data(), bytes.size());
    encoded.erase(encoded.find_last_not_of('=') + 1);
    return encoded;
}

// canonical only: what encode_base64 writes back
Result<std::vector<uint8_t>> decode_base64(std::string_view text)
{
    auto bytes = str::from_base64(text);
    if (!bytes || encode_base64(*bytes) != text)
        return std::unexpected(Error(invalid_argument, "invalid PHC base64 '{}'", text));
    return bytes;
}

// canonical only: decimal without sign nor leading zero
Result<uint32_t> decode_number(std::string_view text)
{
    auto number = str::convert_from_string<uint32_t>(text);
    if (!number || fmt::to_string(*number) != text)
        return std::unexpected(Error(invalid_argument, "invalid PHC number '{}'", text));
    return number;
}

std::unexpected<Error> params_mismatch(std::string_view field, std::span<const std::string_view> keys)
{
    return std::unexpected(
        Error(invalid_argument, "PHC parameters '{}' do not match '{}'", field, fmt::join(keys, ",")));
}

Result<std::vector<uint32_t>> decode_values(std::string_view field, std::span<const std::string_view> keys)
{
    Splitter splitter(',');
    splitter.set_empty_delimitations(true);
    const std::vector<std::string_view> tokens = splitter.split_view(field);
    if (tokens.size() != keys.size())
        return params_mismatch(field, keys);

    std::vector<uint32_t> values;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        if (!tokens[i].starts_with(keys[i]))
            return params_mismatch(field, keys);
        auto value = decode_number(tokens[i].substr(keys[i].size()));
        SIHD_UNEXPECTED_RETURN(value);
        values.push_back(*value);
    }
    return values;
}

Result<PasswordHash::Params> decode_params(Algorithm algorithm, std::string_view field, size_t hash_length)
{
    if (algorithm == Algorithm::argon2id)
    {
        auto values = decode_values(field, argon2_keys);
        SIHD_UNEXPECTED_RETURN(values);
        return Argon2Params {(*values)[0], (*values)[1], (*values)[2]};
    }
    if (algorithm == Algorithm::scrypt)
    {
        auto values = decode_values(field, scrypt_keys);
        SIHD_UNEXPECTED_RETURN(values);
        return ScryptParams {(*values)[0], (*values)[1], (*values)[2]};
    }

    const bool has_length = field.find(',') != std::string_view::npos;
    auto values = has_length ? decode_values(field, pbkdf2_length_keys) : decode_values(field, pbkdf2_keys);
    SIHD_UNEXPECTED_RETURN(values);
    if (has_length && (*values)[1] != hash_length)
    {
        return std::unexpected(
            Error(invalid_argument, "pbkdf2 length {} differs from the hash length {}", (*values)[1], hash_length));
    }
    return Pbkdf2Params {(*values)[0]};
}

} // namespace

PasswordHash::PasswordHash(Algorithm algorithm, Params params, std::vector<uint8_t> salt, std::vector<uint8_t> hash):
    _algorithm(algorithm),
    _params(std::move(params)),
    _salt(std::move(salt)),
    _hash(std::move(hash))
{
}

Result<PasswordHash> PasswordHash::_generate(Algorithm algorithm, Params params, std::string_view password)
{
    std::vector<uint8_t> salt(salt_size);
    ERR_clear_error();
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
        return make_error("RAND_bytes");
    const size_t size = algorithm == Algorithm::pbkdf2_sha512 ? sha512_size : hash_size;
    auto hash = std::visit(Deriver {algorithm, password, salt, size}, params);
    SIHD_UNEXPECTED_RETURN(hash);
    return PasswordHash(algorithm, std::move(params), std::move(salt), std::move(*hash));
}

Result<PasswordHash> PasswordHash::argon2id(std::string_view password, const Argon2Params & params)
{
    return _generate(Algorithm::argon2id, params, password);
}

Result<PasswordHash> PasswordHash::scrypt(std::string_view password, const ScryptParams & params)
{
    return _generate(Algorithm::scrypt, params, password);
}

Result<PasswordHash> PasswordHash::pbkdf2_sha256(std::string_view password, const Pbkdf2Params & params)
{
    return _generate(Algorithm::pbkdf2_sha256, params, password);
}

Result<PasswordHash> PasswordHash::pbkdf2_sha512(std::string_view password, const Pbkdf2Params & params)
{
    return _generate(Algorithm::pbkdf2_sha512, params, password);
}

Result<PasswordHash> PasswordHash::decode(std::string_view phc)
{
    if (!phc.starts_with('$'))
        return std::unexpected(Error(invalid_argument, "PHC string '{}' does not start with '$'", phc));
    Splitter splitter('$');
    splitter.set_empty_delimitations(true);
    const std::vector<std::string_view> fields = splitter.split_view(phc.substr(1));

    const auto known = std::ranges::find(algorithm_ids, fields[0], &AlgorithmId::id);
    if (known == algorithm_ids.end())
        return std::unexpected(Error(not_supported, "unknown PHC algorithm '{}'", fields[0]));

    const size_t expected_fields = known->algorithm == Algorithm::argon2id ? 5 : 4;
    if (fields.size() != expected_fields)
    {
        return std::unexpected(Error(invalid_argument,
                                     "PHC string '{}' has {} fields instead of {}",
                                     phc,
                                     fields.size(),
                                     expected_fields));
    }
    if (known->algorithm == Algorithm::argon2id && fields[1] != fmt::format("v={}", argon2_version))
        return std::unexpected(Error(not_supported, "unsupported argon2 version '{}'", fields[1]));

    const std::string_view params_field = fields[fields.size() - 3];
    const std::string_view salt_field = fields[fields.size() - 2];
    const std::string_view hash_field = fields.back();

    auto salt = decode_base64(salt_field);
    SIHD_UNEXPECTED_RETURN(salt);
    if (salt->size() < min_salt_size)
        return std::unexpected(
            Error(invalid_argument, "PHC salt of {} bytes is under {}", salt->size(), min_salt_size));

    auto hash = decode_base64(hash_field);
    SIHD_UNEXPECTED_RETURN(hash);
    if (hash->size() < min_hash_size)
        return std::unexpected(
            Error(invalid_argument, "PHC hash of {} bytes is under {}", hash->size(), min_hash_size));

    auto params = decode_params(known->algorithm, params_field, hash->size());
    SIHD_UNEXPECTED_RETURN(params);
    return PasswordHash(known->algorithm, std::move(*params), std::move(*salt), std::move(*hash));
}

std::string PasswordHash::encode() const
{
    const auto known = std::ranges::find(algorithm_ids, _algorithm, &AlgorithmId::algorithm);
    const std::string version = _algorithm == Algorithm::argon2id ? fmt::format("$v={}", argon2_version) : "";
    return fmt::format("${}{}${}${}${}",
                       known->id,
                       version,
                       std::visit(ParamsFormatter {}, _params),
                       encode_base64(_salt),
                       encode_base64(_hash));
}

Result<bool> PasswordHash::verify(std::string_view password) const
{
    auto hash = std::visit(Deriver {_algorithm, password, _salt, _hash.size()}, _params);
    SIHD_UNEXPECTED_RETURN(hash);
    return CRYPTO_memcmp(hash->data(), _hash.data(), _hash.size()) == 0;
}

PasswordHash::Algorithm PasswordHash::algorithm() const
{
    return _algorithm;
}

const PasswordHash::Params & PasswordHash::params() const
{
    return _params;
}

const std::vector<uint8_t> & PasswordHash::salt() const
{
    return _salt;
}

const std::vector<uint8_t> & PasswordHash::hash() const
{
    return _hash;
}

} // namespace sihd::crypto
