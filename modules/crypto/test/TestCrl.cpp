#include <gtest/gtest.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/Crl.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/util/Logger.hpp>

#include "crypto_test_helpers.hpp"

using namespace sihd::util;

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

class TestCrl: public ::testing::Test
{
    protected:
        TestCrl() { LoggerManager::stream(); }
        ~TestCrl() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            _ca = make_ca(rsa_key(), "ca.example");
            CertOptions opts;
            opts.common_name = "revoked.example";
            _revoked = make_leaf(_ca, rsa_key(), opts);
            opts.common_name = "good.example";
            _good = make_leaf(_ca, rsa_key(), opts);
            ASSERT_TRUE(_revoked);
            ASSERT_TRUE(_good);
        }

        Crl load(const CrlContent & content) const
        {
            const std::vector<uint8_t> der = make_crl_der(_ca, rsa_key(), content);
            Crl crl;
            EXPECT_TRUE(crl.load_der(der.data(), der.size()).has_value());
            return crl;
        }

        void expect_error(const Result<void> & res, const std::string & message) const
        {
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error().code, ErrorCode::invalid_argument);
            EXPECT_EQ(res.error().message, message);
        }

        Certificate _ca;
        Certificate _revoked;
        Certificate _good;
};

TEST_F(TestCrl, status_lists_revoked_serial)
{
    Crl crl = this->load({.revoked = {&_revoked}});
    EXPECT_TRUE(crl.verify(_ca).has_value());

    auto revoked = crl.status(_revoked);
    ASSERT_TRUE(revoked.has_value());
    EXPECT_EQ(revoked->status, RevocationStatus::revoked);
    EXPECT_EQ(revoked->source, RevocationSource::crl);
    EXPECT_EQ(revoked->revocation_time.seconds(), crl.this_update().seconds());
    EXPECT_EQ(crl.next_update().seconds() - crl.this_update().seconds(), 3600);

    auto good = crl.status(_good);
    ASSERT_TRUE(good.has_value());
    EXPECT_EQ(good->status, RevocationStatus::good);
    EXPECT_EQ(good->revocation_time.seconds(), 0);
}

TEST_F(TestCrl, status_unknown_for_foreign_issuer)
{
    Crl crl = this->load({.revoked = {&_revoked}});
    Certificate stranger = make_ca(other_rsa_key(), "stranger.example");

    auto info = crl.status(stranger);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, RevocationStatus::unknown);
}

TEST_F(TestCrl, verify_rejects_other_issuer)
{
    Crl crl = this->load({});
    Certificate stranger = make_ca(other_rsa_key(), "stranger.example");
    this->expect_error(crl.verify(stranger), "CRL was not issued by 'stranger.example'");

    Certificate impostor = make_ca(other_rsa_key(), "ca.example");
    this->expect_error(crl.verify(impostor), "CRL signature does not match 'ca.example'");
}

TEST_F(TestCrl, verify_rejects_outdated_crl)
{
    Crl expired = this->load({.revoked = {}, .this_update_offset = -7200, .next_update_offset = -3600});
    this->expect_error(expired.verify(_ca), "CRL is outside its validity period");

    Crl future = this->load({.revoked = {}, .this_update_offset = 3600, .next_update_offset = 7200});
    this->expect_error(future.verify(_ca), "CRL is outside its validity period");
}

TEST_F(TestCrl, empty_and_garbage_refused)
{
    Crl crl;
    EXPECT_FALSE(crl);
    EXPECT_EQ(crl.verify(_ca).error().code, ErrorCode::not_initialized);
    EXPECT_EQ(crl.status(_good).error().code, ErrorCode::not_initialized);

    const std::vector<uint8_t> garbage {0x30, 0x03, 0x01, 0x02, 0x03};
    EXPECT_FALSE(crl.load_der(garbage.data(), garbage.size()).has_value());
    EXPECT_FALSE(crl);
}

} // namespace test
