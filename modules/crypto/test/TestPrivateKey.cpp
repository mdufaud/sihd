#include <gtest/gtest.h>

#include <cstdio>

#include <sihd/util/Logger.hpp>

#include <sihd/crypto/PrivateKey.hpp>

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
    EXPECT_TRUE(key2.load_pem_string(pem));
    EXPECT_TRUE(key2);

    EXPECT_EQ(key2.to_pem_string(), pem);
}

TEST_F(TestPrivateKey, pem_file_roundtrip)
{
    std::string path = "/tmp/sihd_test_key.pem";

    PrivateKey key;
    EXPECT_TRUE(key.generate_rsa(2048));
    EXPECT_TRUE(key.save_pem(path));

    PrivateKey key2;
    EXPECT_TRUE(key2.load_pem(path));
    EXPECT_EQ(key2.to_pem_string(), key.to_pem_string());

    std::remove(path.c_str());
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
