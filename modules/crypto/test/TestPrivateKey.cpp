#include <chrono>
#include <cstdio>
#include <filesystem>

#include <gtest/gtest.h>

#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_NEW_LOGGER("sihd::test");
using namespace sihd::crypto;

class TestPrivateKey: public ::testing::Test
{
    protected:
        TestPrivateKey() { sihd::util::LoggerManager::stream(); }
        ~TestPrivateKey() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestPrivateKey, generate_rsa)
{
    PrivateKey key;
    EXPECT_FALSE(key);
    EXPECT_TRUE(key.generate_rsa(2048));
    EXPECT_TRUE(key);
}

TEST_F(TestPrivateKey, generate_ec)
{
    PrivateKey key;
    EXPECT_TRUE(key.generate_ec("prime256v1"));
    EXPECT_TRUE(key);
}

TEST_F(TestPrivateKey, pem_roundtrip)
{
    PrivateKey key;
    EXPECT_TRUE(key.generate_rsa(2048));

    std::string pem = key.to_pem_string();
    EXPECT_FALSE(pem.empty());

    PrivateKey key2;
    EXPECT_TRUE(key2.load_pem_string(pem).has_value());
    EXPECT_TRUE(key2);

    EXPECT_EQ(key2.to_pem_string(), pem);
}

TEST_F(TestPrivateKey, pem_file_roundtrip)
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string
        path = (std::filesystem::temp_directory_path() / ("sihd_test_key_" + std::to_string(unique) + ".pem")).string();

    PrivateKey key;
    EXPECT_TRUE(key.generate_rsa(2048));
    EXPECT_TRUE(key.save_pem(path).has_value());

    PrivateKey key2;
    EXPECT_TRUE(key2.load_pem(path).has_value());
    EXPECT_EQ(key2.to_pem_string(), key.to_pem_string());

    std::remove(path.c_str());
}

TEST_F(TestPrivateKey, load_pem_not_found)
{
    PrivateKey key;
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = (std::filesystem::temp_directory_path() / ("sihd_test_no_key_" + std::to_string(unique) + ".pem"))
                          .string();
    const auto res = key.load_pem(path);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, not_found);

    const auto bad = key.load_pem_string("not a pem");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code, invalid_argument);
}

TEST_F(TestPrivateKey, copy)
{
    PrivateKey key;
    EXPECT_TRUE(key.generate_rsa(2048));

    PrivateKey copy = key;
    EXPECT_TRUE(copy);
    EXPECT_EQ(copy.native(), key.native());
}

TEST_F(TestPrivateKey, move)
{
    PrivateKey key;
    EXPECT_TRUE(key.generate_rsa(2048));
    void *ptr = key.native();

    PrivateKey moved = std::move(key);
    EXPECT_TRUE(moved);
    EXPECT_FALSE(key);
    EXPECT_EQ(moved.native(), ptr);
}

TEST_F(TestPrivateKey, clear)
{
    PrivateKey key;
    EXPECT_TRUE(key.generate_rsa(2048));
    key.clear();
    EXPECT_FALSE(key);
}

} // namespace test
