#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/hash.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");
using namespace sihd::util;

class TestHash: public ::testing::Test
{
    protected:
        TestHash() { sihd::util::LoggerManager::stream(); }
        virtual ~TestHash() { sihd::util::LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}

        static std::string to_hex(const uint8_t digest[20])
        {
            std::ostringstream oss;
            for (int i = 0; i < 20; ++i)
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
            return oss.str();
        }
};

TEST_F(TestHash, test_hash_sha1_empty)
{
    uint8_t digest[20];
    hash::sha1(nullptr, 0, digest);
    // RFC 3174 vector: SHA1("") = da39a3ee5e6b4b0d3255bfef95601890afd80709
    EXPECT_EQ(to_hex(digest), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST_F(TestHash, test_hash_sha1_known_vectors)
{
    uint8_t digest[20];

    // RFC 3174: SHA1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d
    const std::string abc = "abc";
    hash::sha1(reinterpret_cast<const uint8_t *>(abc.data()), abc.size(), digest);
    EXPECT_EQ(to_hex(digest), "a9993e364706816aba3e25717850c26c9cd0d89d");

    // RFC 3174: SHA1("The quick brown fox jumps over the lazy dog")
    //           = 2fd4e1c67a2d28fced849ee1bb76e7391b93eb12
    const std::string fox = "The quick brown fox jumps over the lazy dog";
    hash::sha1(reinterpret_cast<const uint8_t *>(fox.data()), fox.size(), digest);
    EXPECT_EQ(to_hex(digest), "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

} // namespace test
