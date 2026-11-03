#include <string>
#include <vector>

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
            _revoked = this->issue("revoked.example");
            _good = this->issue("good.example");
            ASSERT_TRUE(_revoked);
            ASSERT_TRUE(_good);
        }

        Certificate issue(const std::string & name) const
        {
            CertOptions opts;
            opts.common_name = name;
            opts.crl_urls = {"http://crl.test/1.crl"};
            return make_leaf(_ca, rsa_key(), other_rsa_key(), opts);
        }

        Crl load(const CrlContent & content) const
        {
            const std::vector<uint8_t> der = make_crl_der(_ca, rsa_key(), content);
            Crl crl;
            EXPECT_TRUE(crl.load_der(der.data(), der.size()).has_value());
            return crl;
        }

        void expect_status(const Crl & crl, const Certificate & cert, RevocationStatus status) const
        {
            auto info = crl.status(cert);
            ASSERT_TRUE(info.has_value()) << info.error().message;
            EXPECT_EQ(info->status, status);
            EXPECT_EQ(info->source, RevocationSource::crl);
        }

        template <typename T>
        void expect_error(const Result<T> & res, ErrorCode code, const std::string & message) const
        {
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error().code, code);
            EXPECT_EQ(res.error().message, message);
        }

        Certificate _ca;
        Certificate _revoked;
        Certificate _good;
};

TEST_F(TestCrl, status_lists_revoked_serial)
{
    const Crl crl = this->load({.revoked = {&_revoked}});

    auto info = crl.status(_revoked);

    ASSERT_TRUE(crl.verify(_ca).has_value());
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, RevocationStatus::revoked);
    EXPECT_EQ(info->source, RevocationSource::crl);
    EXPECT_EQ(info->revocation_time.seconds(), crl.this_update().seconds());
    EXPECT_EQ(crl.next_update().seconds() - crl.this_update().seconds(), 3600);
}

TEST_F(TestCrl, status_good_when_serial_not_listed)
{
    const Crl crl = this->load({.revoked = {&_revoked}});

    auto info = crl.status(_good);

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, RevocationStatus::good);
    EXPECT_EQ(info->revocation_time.seconds(), 0);
}

TEST_F(TestCrl, status_unknown_for_foreign_issuer)
{
    const Crl crl = this->load({.revoked = {&_revoked}});
    const Certificate stranger = make_ca(other_rsa_key(), "stranger.example");

    this->expect_status(crl, stranger, RevocationStatus::unknown);
}

TEST_F(TestCrl, partition_covers_its_distribution_point)
{
    const Crl crl = this->load({.revoked = {&_revoked}, .partition_urls = {"http://crl.test/1.crl"}});

    this->expect_status(crl, _revoked, RevocationStatus::revoked);
}

TEST_F(TestCrl, partition_ignores_other_distribution_points)
{
    const Crl crl = this->load({.revoked = {}, .partition_urls = {"http://crl.test/2.crl"}});

    this->expect_status(crl, _revoked, RevocationStatus::unknown);
}

TEST_F(TestCrl, verify_rejects_other_issuer)
{
    const Crl crl = this->load({});
    const Certificate stranger = make_ca(other_rsa_key(), "stranger.example");

    this->expect_error(crl.verify(stranger), ErrorCode::invalid_argument, "CRL was not issued by 'stranger.example'");
}

TEST_F(TestCrl, verify_rejects_forged_signature)
{
    const Crl crl = this->load({});
    const Certificate impostor = make_ca(other_rsa_key(), "ca.example");

    this->expect_error(crl.verify(impostor), ErrorCode::invalid_argument, "CRL signature does not match 'ca.example'");
}

TEST_F(TestCrl, verify_rejects_expired_crl)
{
    const Crl expired = this->load({.revoked = {}, .this_update_offset = -7200, .next_update_offset = -3600});

    this->expect_error(expired.verify(_ca), ErrorCode::invalid_argument, "CRL is outside its validity period");
}

TEST_F(TestCrl, verify_rejects_future_crl)
{
    const Crl future = this->load({.revoked = {}, .this_update_offset = 3600, .next_update_offset = 7200});

    this->expect_error(future.verify(_ca), ErrorCode::invalid_argument, "CRL is outside its validity period");
}

TEST_F(TestCrl, verify_accepts_crl_without_next_update)
{
    const Crl crl = this->load({.revoked = {}, .this_update_offset = -7200, .has_next_update = false});

    EXPECT_TRUE(crl.verify(_ca).has_value());
    EXPECT_EQ(crl.next_update().seconds(), 0);
}

TEST_F(TestCrl, verify_rejects_delta_crl)
{
    const Crl delta = this->load({.revoked = {}, .delta = true});

    this->expect_error(delta.verify(_ca), ErrorCode::not_supported, "delta CRLs are not supported");
}

TEST_F(TestCrl, empty_crl_refused)
{
    const Crl crl;
    const Crl loaded = this->load({});
    EXPECT_FALSE(crl);

    this->expect_error(crl.verify(_ca), ErrorCode::not_initialized, "cannot verify a CRL without CRL and issuer");
    this->expect_error(crl.status(_good),
                       ErrorCode::not_initialized,
                       "cannot look up a certificate without CRL and certificate");
    this->expect_error(loaded.status(Certificate()),
                       ErrorCode::not_initialized,
                       "cannot look up a certificate without CRL and certificate");
}

TEST_F(TestCrl, garbage_refused)
{
    const std::vector<uint8_t> garbage {0x30, 0x03, 0x01, 0x02, 0x03};
    Crl crl;

    auto loaded = crl.load_der(garbage.data(), garbage.size());

    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, ErrorCode::invalid_argument);
    // the tail is the OpenSSL error text
    EXPECT_TRUE(loaded.error().message.starts_with("read CRL DER data: ")) << loaded.error().message;
    EXPECT_FALSE(crl);
}

} // namespace test
