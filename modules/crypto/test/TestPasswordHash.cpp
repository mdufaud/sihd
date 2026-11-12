#include <string>
#include <string_view>
#include <variant>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/crypto/PasswordHash.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;
using namespace sihd::util;

namespace
{

// made by argon2-cffi and python hashlib: password "password", salt "sihd-salt-16byte"
constexpr std::string_view
    argon2id_vector = "$argon2id$v=19$m=64,t=1,p=2$c2loZC1zYWx0LTE2Ynl0ZQ$uypo2vBpqLakYB8cNNbA/Ua6m6mRc/H4wYAr3Ey4LBQ";
constexpr std::string_view
    scrypt_vector = "$scrypt$ln=4,r=8,p=1$c2loZC1zYWx0LTE2Ynl0ZQ$nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rcg";
constexpr std::string_view
    pbkdf2_sha256_vector = "$pbkdf2-sha256$i=1000$c2loZC1zYWx0LTE2Ynl0ZQ$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA";
constexpr std::string_view
    pbkdf2_sha512_vector = "$pbkdf2-sha512$i=1000$c2loZC1zYWx0LTE2Ynl0ZQ$1ElQPGnETqG4uEC8eOgd3i49DqxdneAlwSALnaYC/"
                           "DSPGAizqx/4QCeChRZa2CcggY2aU/iOEfzTudxxq+kyGA";

constexpr std::string_view vector_salt = "sihd-salt-16byte";
constexpr std::string_view vector_hash_b64 = "X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA";

} // namespace

class TestPasswordHash: public ::testing::Test
{
    protected:
        TestPasswordHash() { LoggerManager::stream(); }
        ~TestPasswordHash() { LoggerManager::clear_loggers(); }

        static void expect_vector(std::string_view phc, PasswordHash::Algorithm algorithm)
        {
            auto decoded = PasswordHash::decode(phc);
            ASSERT_TRUE(decoded.has_value()) << decoded.error().message;

            EXPECT_EQ(decoded->algorithm(), algorithm);
            EXPECT_EQ(decoded->encode(), phc);
            EXPECT_EQ(decoded->verify("password"), true);
            EXPECT_EQ(decoded->verify("Password"), false);
        }

        static void expect_round_trip(const Result<PasswordHash> & generated, size_t hash_size)
        {
            ASSERT_TRUE(generated.has_value()) << generated.error().message;
            EXPECT_EQ(generated->salt().size(), 16u);
            EXPECT_EQ(generated->hash().size(), hash_size);

            auto decoded = PasswordHash::decode(generated->encode());
            ASSERT_TRUE(decoded.has_value()) << decoded.error().message;

            EXPECT_EQ(decoded->algorithm(), generated->algorithm());
            EXPECT_EQ(decoded->encode(), generated->encode());
            EXPECT_EQ(decoded->verify("secret"), true);
            EXPECT_EQ(decoded->verify("Secret"), false);
        }

        static void expect_decode_error(std::string_view phc, ErrorCode code, std::string_view message)
        {
            auto decoded = PasswordHash::decode(phc);
            ASSERT_FALSE(decoded.has_value()) << phc;

            EXPECT_EQ(decoded.error().code, code) << phc;
            EXPECT_EQ(decoded.error().message, message);
        }
};

TEST_F(TestPasswordHash, test_argon2id_reference_vector)
{
    expect_vector(argon2id_vector, PasswordHash::Algorithm::argon2id);
}

TEST_F(TestPasswordHash, test_scrypt_reference_vector)
{
    expect_vector(scrypt_vector, PasswordHash::Algorithm::scrypt);
}

TEST_F(TestPasswordHash, test_pbkdf2_sha256_reference_vector)
{
    expect_vector(pbkdf2_sha256_vector, PasswordHash::Algorithm::pbkdf2_sha256);
}

TEST_F(TestPasswordHash, test_pbkdf2_sha512_reference_vector)
{
    expect_vector(pbkdf2_sha512_vector, PasswordHash::Algorithm::pbkdf2_sha512);
}

TEST_F(TestPasswordHash, test_pbkdf2_reads_its_length_parameter)
{
    const std::string phc = fmt::format("$pbkdf2-sha256$i=1000,l=32$c2loZC1zYWx0LTE2Ynl0ZQ${}", vector_hash_b64);

    auto decoded = PasswordHash::decode(phc);
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message;

    EXPECT_EQ(decoded->verify("password"), true);
    EXPECT_EQ(decoded->encode(), pbkdf2_sha256_vector);
}

TEST_F(TestPasswordHash, test_decode_reads_params_and_salt)
{
    auto decoded = PasswordHash::decode(argon2id_vector);
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message;

    const auto *params = std::get_if<Argon2Params>(&decoded->params());
    ASSERT_NE(params, nullptr);
    EXPECT_EQ(params->memory_kib, 64u);
    EXPECT_EQ(params->iterations, 1u);
    EXPECT_EQ(params->parallelism, 2u);
    EXPECT_EQ(std::string(decoded->salt().begin(), decoded->salt().end()), vector_salt);
    EXPECT_EQ(decoded->hash().size(), 32u);
}

TEST_F(TestPasswordHash, test_generated_hashes_round_trip)
{
    expect_round_trip(PasswordHash::argon2id("secret", {.memory_kib = 64, .iterations = 1, .parallelism = 1}), 32);
    expect_round_trip(PasswordHash::scrypt("secret", {.log2_n = 4, .block_size = 8, .parallelism = 1}), 32);
    expect_round_trip(PasswordHash::pbkdf2_sha256("secret", {.iterations = 1000}), 32);
    expect_round_trip(PasswordHash::pbkdf2_sha512("secret", {.iterations = 1000}), 64);
}

TEST_F(TestPasswordHash, test_empty_password_round_trips)
{
    auto generated = PasswordHash::pbkdf2_sha256("", {.iterations = 1000});
    ASSERT_TRUE(generated.has_value()) << generated.error().message;

    EXPECT_EQ(generated->verify(""), true);
    EXPECT_EQ(generated->verify("secret"), false);
}

TEST_F(TestPasswordHash, test_default_params)
{
    auto argon2id = PasswordHash::argon2id("secret");
    auto scrypt = PasswordHash::scrypt("secret");
    auto pbkdf2 = PasswordHash::pbkdf2_sha256("secret");
    ASSERT_TRUE(argon2id.has_value()) << argon2id.error().message;
    ASSERT_TRUE(scrypt.has_value()) << scrypt.error().message;
    ASSERT_TRUE(pbkdf2.has_value()) << pbkdf2.error().message;

    EXPECT_EQ(argon2id->encode().substr(0, 31), "$argon2id$v=19$m=19456,t=2,p=1$");
    EXPECT_EQ(scrypt->encode().substr(0, 22), "$scrypt$ln=17,r=8,p=1$");
    EXPECT_EQ(pbkdf2->encode().substr(0, 24), "$pbkdf2-sha256$i=600000$");
    EXPECT_EQ(argon2id->verify("secret"), true);
}

TEST_F(TestPasswordHash, test_salt_is_random)
{
    const Argon2Params params {.memory_kib = 64, .iterations = 1, .parallelism = 1};

    auto first = PasswordHash::argon2id("secret", params);
    auto second = PasswordHash::argon2id("secret", params);
    ASSERT_TRUE(first.has_value() && second.has_value());

    EXPECT_NE(first->salt(), second->salt());
    EXPECT_NE(first->hash(), second->hash());
}

TEST_F(TestPasswordHash, test_invalid_costs_are_refused)
{
    auto argon2id = PasswordHash::argon2id("secret", {.memory_kib = 4, .iterations = 1, .parallelism = 1});
    auto scrypt = PasswordHash::scrypt("secret", {.log2_n = 64, .block_size = 8, .parallelism = 1});
    auto pbkdf2 = PasswordHash::pbkdf2_sha512("secret", {.iterations = 0});
    ASSERT_FALSE(argon2id.has_value());
    ASSERT_FALSE(scrypt.has_value());
    ASSERT_FALSE(pbkdf2.has_value());

    EXPECT_EQ(argon2id.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(argon2id.error().message, "invalid argon2id params 'm=4,t=1,p=1'");
    EXPECT_EQ(scrypt.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(scrypt.error().message, "invalid scrypt params 'ln=64,r=8,p=1'");
    EXPECT_EQ(pbkdf2.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(pbkdf2.error().message, "invalid pbkdf2 params 'i=0'");
}

TEST_F(TestPasswordHash, test_verify_reports_a_failed_derivation)
{
    auto decoded = PasswordHash::decode(
        "$scrypt$ln=63,r=8,p=1$c2loZC1zYWx0LTE2Ynl0ZQ$nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rcg");
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message;

    auto verified = decoded->verify("password");

    ASSERT_FALSE(verified.has_value());
    EXPECT_EQ(verified.error().code, ErrorCode::io_error);
}

TEST_F(TestPasswordHash, test_decode_refuses_unsupported_hashes)
{
    expect_decode_error("$argon2i$v=19$m=64,t=1,p=2$c2loZC1zYWx0LTE2Ynl0ZQ$uypo2vBpqLakYB8cNNbA",
                        ErrorCode::not_supported,
                        "unknown PHC algorithm 'argon2i'");
    expect_decode_error("$1$c2loZC1zYWx0$uypo2vBpqLakYB8cNNbA", ErrorCode::not_supported, "unknown PHC algorithm '1'");
    expect_decode_error("$argon2id$v=16$m=64,t=1,p=2$c2loZC1zYWx0LTE2Ynl0ZQ$uypo2vBpqLakYB8cNNbA",
                        ErrorCode::not_supported,
                        "unsupported argon2 version 'v=16'");
}

TEST_F(TestPasswordHash, test_decode_refuses_malformed_fields)
{
    expect_decode_error("scrypt", ErrorCode::invalid_argument, "PHC string 'scrypt' does not start with '$'");
    expect_decode_error("$scrypt$ln=4,r=8,p=1$c2loZC1zYWx0LTE2Ynl0ZQ",
                        ErrorCode::invalid_argument,
                        "PHC string '$scrypt$ln=4,r=8,p=1$c2loZC1zYWx0LTE2Ynl0ZQ' has 3 fields instead of 4");
    expect_decode_error("$pbkdf2-sha256$i=1000$c2loZC1zYWx0LTE2Ynl0ZQ==$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA",
                        ErrorCode::invalid_argument,
                        "invalid PHC base64 'c2loZC1zYWx0LTE2Ynl0ZQ=='");
    // the last character carries non-zero padding bits
    expect_decode_error("$scrypt$ln=4,r=8,p=1$c2loZC1zYWx0LTE2Ynl0ZQ$nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rch",
                        ErrorCode::invalid_argument,
                        "invalid PHC base64 'nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rch'");
}

TEST_F(TestPasswordHash, test_decode_refuses_short_salts_and_hashes)
{
    expect_decode_error("$pbkdf2-sha256$i=1000$AAAA$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA",
                        ErrorCode::invalid_argument,
                        "PHC salt of 3 bytes is under 8");
    expect_decode_error("$pbkdf2-sha256$i=1000$c2loZC1zYWx0LTE2Ynl0ZQ$X9C1",
                        ErrorCode::invalid_argument,
                        "PHC hash of 3 bytes is under 4");
    expect_decode_error("$pbkdf2-sha256$i=1000$c2loZC1zYWx0LTE2Ynl0ZQ$",
                        ErrorCode::invalid_argument,
                        "PHC hash of 0 bytes is under 4");
}

TEST_F(TestPasswordHash, test_decode_refuses_malformed_params)
{
    expect_decode_error("$scrypt$r=8,ln=4,p=1$c2loZC1zYWx0LTE2Ynl0ZQ$nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rcg",
                        ErrorCode::invalid_argument,
                        "PHC parameters 'r=8,ln=4,p=1' do not match 'ln=,r=,p='");
    expect_decode_error("$scrypt$ln=4,r=8$c2loZC1zYWx0LTE2Ynl0ZQ$nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rcg",
                        ErrorCode::invalid_argument,
                        "PHC parameters 'ln=4,r=8' do not match 'ln=,r=,p='");
    expect_decode_error(
        "$pbkdf2-sha256$i=1000,l=32,x=1$c2loZC1zYWx0LTE2Ynl0ZQ$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA",
        ErrorCode::invalid_argument,
        "PHC parameters 'i=1000,l=32,x=1' do not match 'i=,l='");
    expect_decode_error("$pbkdf2-sha256$i=1000,l=16$c2loZC1zYWx0LTE2Ynl0ZQ$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA",
                        ErrorCode::invalid_argument,
                        "pbkdf2 length 16 differs from the hash length 32");
}

TEST_F(TestPasswordHash, test_decode_refuses_non_canonical_numbers)
{
    expect_decode_error("$scrypt$ln=04,r=8,p=1$c2loZC1zYWx0LTE2Ynl0ZQ$nJ6E3o2+iZeJ+yfRCtmzcKp6im8asRm3pu8VkG/8rcg",
                        ErrorCode::invalid_argument,
                        "invalid PHC number '04'");
    expect_decode_error("$pbkdf2-sha256$i=-5$c2loZC1zYWx0LTE2Ynl0ZQ$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA",
                        ErrorCode::invalid_argument,
                        "invalid PHC number '-5'");
    expect_decode_error(
        "$pbkdf2-sha256$i=4294967296$c2loZC1zYWx0LTE2Ynl0ZQ$X9C1kBVP4gMHcvD3G92qiMNcjAMdpb5KkIL4j5mGyOA",
        ErrorCode::invalid_argument,
        "invalid PHC number '4294967296'");
}

} // namespace test
