#include <gtest/gtest.h>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/crypto/cipher.hpp>
#include <sihd/crypto/digest.hpp>

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
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };
    const uint8_t iv[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };

    std::string plaintext = "Hello, OpenSSL cipher test!";
    auto data = reinterpret_cast<const uint8_t *>(plaintext.data());

    auto encrypted = sihd::crypto::cipher::encrypt("AES-256-CBC", key, 32, iv, 16, data, plaintext.size());
    ASSERT_FALSE(encrypted.empty());
    EXPECT_NE(encrypted.size(), plaintext.size());

    auto decrypted = sihd::crypto::cipher::decrypt("AES-256-CBC", key, 32, iv, 16, encrypted.data(), encrypted.size());
    ASSERT_EQ(decrypted.size(), plaintext.size());

    std::string result(reinterpret_cast<const char *>(decrypted.data()), decrypted.size());
    EXPECT_EQ(result, plaintext);
}

TEST_F(TestCipherUtils, aes_256_cbc_roundtrip_arrview)
{
    const uint8_t key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };
    const uint8_t iv[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };

    using sihd::util::ArrByteView;
    std::string plaintext = "Hello, OpenSSL cipher test!";

    auto encrypted = sihd::crypto::cipher::encrypt("AES-256-CBC",
                                                   ArrByteView(key, sizeof(key)),
                                                   ArrByteView(iv, sizeof(iv)),
                                                   ArrByteView(plaintext.data(), plaintext.size()));
    ASSERT_FALSE(encrypted.empty());

    auto decrypted = sihd::crypto::cipher::decrypt("AES-256-CBC",
                                                   ArrByteView(key, sizeof(key)),
                                                   ArrByteView(iv, sizeof(iv)),
                                                   ArrByteView(encrypted.data(), encrypted.size()));
    std::string result(reinterpret_cast<const char *>(decrypted.data()), decrypted.size());
    EXPECT_EQ(result, plaintext);
}

TEST_F(TestCipherUtils, sha256_arrview)
{
    using sihd::util::ArrByteView;
    std::string input = "abc";
    auto hash = sihd::crypto::digest::compute("SHA-256", ArrByteView(input.data(), input.size()));
    ASSERT_EQ(hash.size(), 32u);
    EXPECT_EQ(hash[0], 0xba);
    EXPECT_EQ(hash[1], 0x78);
    EXPECT_EQ(hash[31], 0xad);
}

TEST_F(TestCipherUtils, sha256_known_vector)
{
    std::string input = "abc";
    auto hash = sihd::crypto::digest::compute("SHA-256",
                                              reinterpret_cast<const uint8_t *>(input.data()),
                                              input.size());
    ASSERT_EQ(hash.size(), 32u);

    // SHA-256("abc") = ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
    EXPECT_EQ(hash[0], 0xba);
    EXPECT_EQ(hash[1], 0x78);
    EXPECT_EQ(hash[2], 0x16);
    EXPECT_EQ(hash[3], 0xbf);
    EXPECT_EQ(hash[31], 0xad);
}

TEST_F(TestCipherUtils, sha512_known_vector)
{
    std::string input = "abc";
    auto hash = sihd::crypto::digest::compute("SHA-512",
                                              reinterpret_cast<const uint8_t *>(input.data()),
                                              input.size());
    ASSERT_EQ(hash.size(), 64u);

    // SHA-512("abc") starts with ddaf35a1...
    EXPECT_EQ(hash[0], 0xdd);
    EXPECT_EQ(hash[1], 0xaf);
    EXPECT_EQ(hash[2], 0x35);
    EXPECT_EQ(hash[3], 0xa1);
}

TEST_F(TestCipherUtils, unknown_algorithm)
{
    auto hash = sihd::crypto::digest::compute("UNKNOWN-ALG", nullptr, 0);
    EXPECT_TRUE(hash.empty());
}

} // namespace test
