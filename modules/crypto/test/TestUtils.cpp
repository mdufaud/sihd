#include <gtest/gtest.h>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/utils.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

class TestUtils: public ::testing::Test
{
    protected:
        TestUtils() { sihd::util::LoggerManager::stream(); }
        ~TestUtils() { sihd::util::LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            ASSERT_TRUE(_key.generate_rsa(2048));
            ASSERT_TRUE(_cert.generate_self_signed(_key, "test.local"));
        }

        PrivateKey _key;
        Certificate _cert;
};

TEST_F(TestUtils, sign_verify_roundtrip)
{
    std::string message = "test data to sign";
    auto data = reinterpret_cast<const uint8_t *>(message.data());

    auto signature = utils::sign_data(_key, data, message.size());
    ASSERT_FALSE(signature.empty());

    EXPECT_TRUE(utils::verify_data(_cert, data, message.size(), signature.data(), signature.size()));
}

TEST_F(TestUtils, verify_wrong_data)
{
    std::string message = "test data to sign";
    auto data = reinterpret_cast<const uint8_t *>(message.data());

    auto signature = utils::sign_data(_key, data, message.size());
    ASSERT_FALSE(signature.empty());

    std::string wrong = "wrong data";
    auto wrong_data = reinterpret_cast<const uint8_t *>(wrong.data());
    EXPECT_FALSE(utils::verify_data(_cert, wrong_data, wrong.size(), signature.data(), signature.size()));
}

TEST_F(TestUtils, verify_wrong_cert)
{
    std::string message = "test data to sign";
    auto data = reinterpret_cast<const uint8_t *>(message.data());

    auto signature = utils::sign_data(_key, data, message.size());
    ASSERT_FALSE(signature.empty());

    PrivateKey other_key;
    ASSERT_TRUE(other_key.generate_rsa(2048));
    Certificate other_cert;
    ASSERT_TRUE(other_cert.generate_self_signed(other_key, "other.local"));

    EXPECT_FALSE(utils::verify_data(other_cert, data, message.size(), signature.data(), signature.size()));
}

TEST_F(TestUtils, sign_verify_roundtrip_arrview)
{
    using sihd::util::ArrByteView;
    std::string message = "test data to sign";
    ArrByteView data(message.data(), message.size());

    auto signature = utils::sign_data(_key, data);
    ASSERT_FALSE(signature.empty());

    EXPECT_TRUE(utils::verify_data(_cert, data, ArrByteView(signature.data(), signature.size())));

    std::string wrong = "wrong data";
    EXPECT_FALSE(utils::verify_data(_cert,
                                    ArrByteView(wrong.data(), wrong.size()),
                                    ArrByteView(signature.data(), signature.size())));
}

TEST_F(TestUtils, sign_empty_key)
{
    PrivateKey empty;
    auto sig = utils::sign_data(empty, nullptr, 0);
    EXPECT_TRUE(sig.empty());
}

} // namespace test
