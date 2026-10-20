#include <array>

#include <gtest/gtest.h>

#include <sihd/crypto/cipher.hpp>
#include <sihd/crypto/digest.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_LOGGER;

class TestCipherUtils: public ::testing::Test
{
    protected:
        TestCipherUtils() { sihd::util::LoggerManager::stream(); }
        ~TestCipherUtils() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestCipherUtils, aes_256_cbc_roundtrip)
{
    const uint8_t key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };
    const uint8_t iv[16] = {
        0x00,
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0x09,
        0x0a,
        0x0b,
        0x0c,
        0x0d,
        0x0e,
        0x0f,
    };

    std::string plaintext = "Hello, OpenSSL cipher test!";
    auto data = reinterpret_cast<const uint8_t *>(plaintext.data());

    auto encrypted = sihd::crypto::cipher::encrypt("AES-256-CBC", key, 32, iv, 16, data, plaintext.size());
    ASSERT_TRUE(encrypted.has_value());
    EXPECT_NE(encrypted->size(), plaintext.size());

    auto
        decrypted = sihd::crypto::cipher::decrypt("AES-256-CBC", key, 32, iv, 16, encrypted->data(), encrypted->size());
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(decrypted->size(), plaintext.size());

    std::string result(reinterpret_cast<const char *>(decrypted->data()), decrypted->size());
    EXPECT_EQ(result, plaintext);
}

TEST_F(TestCipherUtils, aes_256_cbc_roundtrip_arrview)
{
    const uint8_t key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };
    const uint8_t iv[16] = {
        0x00,
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0x09,
        0x0a,
        0x0b,
        0x0c,
        0x0d,
        0x0e,
        0x0f,
    };

    using sihd::util::ArrByteView;
    std::string plaintext = "Hello, OpenSSL cipher test!";

    auto encrypted = sihd::crypto::cipher::encrypt("AES-256-CBC",
                                                   ArrByteView(key, sizeof(key)),
                                                   ArrByteView(iv, sizeof(iv)),
                                                   ArrByteView(plaintext.data(), plaintext.size()));
    ASSERT_TRUE(encrypted.has_value());

    auto decrypted = sihd::crypto::cipher::decrypt("AES-256-CBC",
                                                   ArrByteView(key, sizeof(key)),
                                                   ArrByteView(iv, sizeof(iv)),
                                                   ArrByteView(encrypted->data(), encrypted->size()));
    ASSERT_TRUE(decrypted.has_value());
    std::string result(reinterpret_cast<const char *>(decrypted->data()), decrypted->size());
    EXPECT_EQ(result, plaintext);
}

TEST_F(TestCipherUtils, sha256_arrview)
{
    using sihd::util::ArrByteView;
    std::string input = "abc";
    auto hash = sihd::crypto::digest::compute("SHA-256", ArrByteView(input.data(), input.size()));
    ASSERT_TRUE(hash.has_value());
    ASSERT_EQ(hash->size(), 32u);
    EXPECT_EQ((*hash)[0], 0xba);
    EXPECT_EQ((*hash)[1], 0x78);
    EXPECT_EQ((*hash)[31], 0xad);
}

TEST_F(TestCipherUtils, sha256_known_vector)
{
    std::string input = "abc";
    auto hash = sihd::crypto::digest::compute("SHA-256", reinterpret_cast<const uint8_t *>(input.data()), input.size());
    ASSERT_TRUE(hash.has_value());
    ASSERT_EQ(hash->size(), 32u);

    // SHA-256("abc") = ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
    EXPECT_EQ((*hash)[0], 0xba);
    EXPECT_EQ((*hash)[1], 0x78);
    EXPECT_EQ((*hash)[2], 0x16);
    EXPECT_EQ((*hash)[3], 0xbf);
    EXPECT_EQ((*hash)[31], 0xad);
}

TEST_F(TestCipherUtils, sha512_known_vector)
{
    std::string input = "abc";
    auto hash = sihd::crypto::digest::compute("SHA-512", reinterpret_cast<const uint8_t *>(input.data()), input.size());
    ASSERT_TRUE(hash.has_value());
    ASSERT_EQ(hash->size(), 64u);

    // SHA-512("abc") starts with ddaf35a1...
    EXPECT_EQ((*hash)[0], 0xdd);
    EXPECT_EQ((*hash)[1], 0xaf);
    EXPECT_EQ((*hash)[2], 0x35);
    EXPECT_EQ((*hash)[3], 0xa1);
}

TEST_F(TestCipherUtils, unknown_algorithm)
{
    const auto hash = sihd::crypto::digest::compute("UNKNOWN-ALG", nullptr, 0);
    ASSERT_FALSE(hash.has_value());
    EXPECT_EQ(hash.error().code, invalid_argument);
    EXPECT_EQ(hash.error().message, "unknown digest 'UNKNOWN-ALG'");

    const uint8_t key[16] = {0};
    const auto encrypted = sihd::crypto::cipher::encrypt("UNKNOWN-CIPHER", key, sizeof(key), key, sizeof(key), key, 16);
    ASSERT_FALSE(encrypted.has_value());
    EXPECT_EQ(encrypted.error().code, invalid_argument);
    EXPECT_EQ(encrypted.error().message, "unknown cipher 'UNKNOWN-CIPHER'");
}

TEST_F(TestCipherUtils, key_and_iv_lengths_are_checked)
{
    const std::array<uint8_t, 32> buf {};
    const std::array<uint8_t, 16> data {};

    auto short_key = sihd::crypto::cipher::encrypt("AES-256-CBC",
                                                   buf.data(),
                                                   16,
                                                   buf.data(),
                                                   16,
                                                   data.data(),
                                                   data.size());
    ASSERT_FALSE(short_key.has_value());
    EXPECT_EQ(short_key.error().code, invalid_argument);
    EXPECT_EQ(short_key.error().message, "cipher 'AES-256-CBC' needs a 32 bytes key, got 16");

    auto long_key = sihd::crypto::cipher::decrypt("AES-256-CBC",
                                                  buf.data(),
                                                  33,
                                                  buf.data(),
                                                  16,
                                                  data.data(),
                                                  data.size());
    ASSERT_FALSE(long_key.has_value());
    EXPECT_EQ(long_key.error().message, "cipher 'AES-256-CBC' needs a 32 bytes key, got 33");

    auto short_iv = sihd::crypto::cipher::encrypt("AES-256-CBC",
                                                  buf.data(),
                                                  32,
                                                  buf.data(),
                                                  8,
                                                  data.data(),
                                                  data.size());
    ASSERT_FALSE(short_iv.has_value());
    EXPECT_EQ(short_iv.error().message, "cipher 'AES-256-CBC' needs a 16 bytes iv, got 8");

    auto no_key = sihd::crypto::cipher::encrypt("AES-256-CBC", nullptr, 32, buf.data(), 16, data.data(), data.size());
    ASSERT_FALSE(no_key.has_value());
    EXPECT_EQ(no_key.error().message, "cipher 'AES-256-CBC' needs a key");

    auto no_iv = sihd::crypto::cipher::encrypt("AES-256-CBC", buf.data(), 32, nullptr, 16, data.data(), data.size());
    ASSERT_FALSE(no_iv.has_value());
    EXPECT_EQ(no_iv.error().message, "cipher 'AES-256-CBC' needs an iv");

    auto ok = sihd::crypto::cipher::encrypt("AES-256-CBC", buf.data(), 32, buf.data(), 16, data.data(), data.size());
    EXPECT_TRUE(ok.has_value());
}

TEST_F(TestCipherUtils, null_cipher_is_refused)
{
    const std::array<uint8_t, 16> buf {};
    auto res = sihd::crypto::cipher::encrypt("NULL", buf.data(), 0, buf.data(), 0, buf.data(), buf.size());
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, invalid_argument);
    EXPECT_EQ(res.error().message, "cipher 'NULL' does not encrypt");
}

} // namespace test
