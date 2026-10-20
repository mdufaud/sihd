#include <string>

#include <gtest/gtest.h>

#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "crypto_test_helpers.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace test
{
SIHD_NEW_LOGGER("sihd::test");
using namespace sihd::crypto;

class TestPrivateKey: public ::testing::Test
{
    protected:
        TestPrivateKey() { LoggerManager::stream(); }
        ~TestPrivateKey() { LoggerManager::clear_loggers(); }
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
    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);

    std::string pem = key.to_pem_string();
    EXPECT_FALSE(pem.empty());

    PrivateKey key2;
    EXPECT_TRUE(key2.load_pem_string(pem).has_value());
    EXPECT_TRUE(key2);

    EXPECT_EQ(key2.to_pem_string(), pem);
}

TEST_F(TestPrivateKey, pem_file_roundtrip)
{
    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine(dir.path(), "key.pem");

    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);
    EXPECT_TRUE(key.save_pem(path).has_value());

    PrivateKey key2;
    EXPECT_TRUE(key2.load_pem(path).has_value());
    EXPECT_EQ(key2.to_pem_string(), key.to_pem_string());
}

TEST_F(TestPrivateKey, load_pem_not_found)
{
    PrivateKey key;
    sihd::sys::TmpDir dir;
    const auto res = key.load_pem(sihd::sys::fs::combine(dir.path(), "missing.pem"));
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, not_found);

    const auto bad = key.load_pem_string("not a pem");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code, invalid_argument);
}

TEST_F(TestPrivateKey, copy)
{
    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);

    PrivateKey copy = key;
    EXPECT_TRUE(copy);
    EXPECT_EQ(copy.native(), key.native());
}

TEST_F(TestPrivateKey, move)
{
    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);
    void *ptr = key.native();

    PrivateKey moved = std::move(key);
    EXPECT_TRUE(moved);
    EXPECT_FALSE(key);
    EXPECT_EQ(moved.native(), ptr);
}

TEST_F(TestPrivateKey, clear)
{
    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);
    key.clear();
    EXPECT_FALSE(key);
}

TEST_F(TestPrivateKey, save_pem_is_owner_only)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "posix permissions";
    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine({dir.path(), "key.pem"});
    ASSERT_TRUE(sihd::sys::fs::write(path, "old").has_value());
    ASSERT_TRUE(sihd::sys::fs::permission_set(path, 0777).has_value());

    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);
    ASSERT_TRUE(key.save_pem(path).has_value());

    EXPECT_EQ(sihd::sys::fs::permission_get(path), 0600u);
}

TEST_F(TestPrivateKey, save_pem_replaces_links_without_writing_through_them)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "posix links";
    sihd::sys::TmpDir dir;
    const std::string target = sihd::sys::fs::combine({dir.path(), "target"});
    const std::string symlink = sihd::sys::fs::combine({dir.path(), "symlink.pem"});
    const std::string hardlink = sihd::sys::fs::combine({dir.path(), "hardlink.pem"});
    ASSERT_TRUE(sihd::sys::fs::write(target, "keep").has_value());
    ASSERT_TRUE(sihd::sys::fs::make_file_link(target, symlink).has_value());
    ASSERT_TRUE(sihd::sys::fs::make_hard_link(target, hardlink).has_value());

    PrivateKey key = rsa_key();
    ASSERT_TRUE(key);
    ASSERT_TRUE(key.save_pem(symlink).has_value());
    ASSERT_TRUE(key.save_pem(hardlink).has_value());

    EXPECT_EQ(sihd::sys::fs::read_all(target).value_or(""), "keep");
    EXPECT_FALSE(sihd::sys::fs::is_symlink(symlink));
    EXPECT_EQ(sihd::sys::fs::read_all(symlink).value_or(""), key.to_pem_string());
    EXPECT_EQ(sihd::sys::fs::read_all(hardlink).value_or(""), key.to_pem_string());
}

} // namespace test
