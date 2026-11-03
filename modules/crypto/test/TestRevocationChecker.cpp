#include <gtest/gtest.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/RevocationChecker.hpp>
#include <sihd/util/Logger.hpp>

#include "crypto_test_helpers.hpp"

using namespace sihd::util;

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

class TestRevocationChecker: public ::testing::Test
{
    protected:
        TestRevocationChecker() { LoggerManager::stream(); }
        ~TestRevocationChecker() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            _ca = make_ca(rsa_key(), "ca.example");
            CertOptions opts;
            opts.common_name = "leaf.example";
            opts.ocsp_urls = {"http://ocsp.test"};
            opts.crl_urls = {"http://crl.test/ca.crl"};
            _leaf = make_leaf(_ca, rsa_key(), opts);
            ASSERT_TRUE(_leaf);

            _checker.set_ocsp_fetcher([this](const std::string & url, ArrUByteView request) {
                ++_ocsp_fetches;
                EXPECT_EQ(url, "http://ocsp.test");
                if (!_ocsp_up)
                    return Result<std::vector<uint8_t>>(std::unexpected(Error(ErrorCode::io_error, "unreachable")));
                const std::vector<uint8_t> der(request.data(), request.data() + request.size());
                return Result<std::vector<uint8_t>>(ocsp_respond(der, _ca, rsa_key()));
            });
            _checker.set_crl_fetcher([this](const std::string & url) {
                ++_crl_fetches;
                EXPECT_EQ(url, "http://crl.test/ca.crl");
                if (!_crl_up)
                    return Result<std::vector<uint8_t>>(std::unexpected(Error(ErrorCode::io_error, "unreachable")));
                return Result<std::vector<uint8_t>>(make_crl_der(_ca, rsa_key(), {.revoked = {&_leaf}}));
            });
        }

        Result<RevocationInfo> check() { return _checker.check({_leaf, _ca}); }

        void expect_info(const Result<RevocationInfo> & info, RevocationStatus status, RevocationSource source) const
        {
            ASSERT_TRUE(info.has_value()) << info.error().message;
            EXPECT_EQ(info->status, status);
            EXPECT_EQ(info->source, source);
        }

        Certificate _ca;
        Certificate _leaf;
        RevocationChecker _checker;
        bool _ocsp_up = true;
        bool _crl_up = true;
        int _ocsp_fetches = 0;
        int _crl_fetches = 0;
};

TEST_F(TestRevocationChecker, ocsp_answers_first)
{
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_ocsp_fetches, 1);
    EXPECT_EQ(_crl_fetches, 0);
}

TEST_F(TestRevocationChecker, ocsp_failure_falls_back_to_crl)
{
    _ocsp_up = false;

    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);

    EXPECT_EQ(_ocsp_fetches, 1);
    EXPECT_EQ(_crl_fetches, 1);
}

TEST_F(TestRevocationChecker, every_source_failing_is_an_error)
{
    _ocsp_up = false;
    _crl_up = false;

    auto info = this->check();

    ASSERT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::io_error);
    EXPECT_EQ(info.error().message, "ocsp http://ocsp.test: unreachable; crl http://crl.test/ca.crl: unreachable");
}

TEST_F(TestRevocationChecker, certificate_without_source_is_unknown)
{
    CertOptions opts;
    opts.common_name = "silent.example";
    Certificate silent = make_leaf(_ca, rsa_key(), opts);

    this->expect_info(_checker.check({silent, _ca}), RevocationStatus::unknown, RevocationSource::none);

    EXPECT_EQ(_ocsp_fetches, 0);
    EXPECT_EQ(_crl_fetches, 0);
}

TEST_F(TestRevocationChecker, unset_fetchers_skip_their_source)
{
    RevocationChecker bare;

    this->expect_info(bare.check({_leaf, _ca}), RevocationStatus::unknown, RevocationSource::none);
}

TEST_F(TestRevocationChecker, answers_are_cached)
{
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);
    EXPECT_EQ(_ocsp_fetches, 1);

    _ocsp_up = false;
    RevocationChecker crl_only;
    crl_only.set_crl_fetcher([this](const std::string &) {
        ++_crl_fetches;
        return Result<std::vector<uint8_t>>(make_crl_der(_ca, rsa_key(), {.revoked = {&_leaf}}));
    });
    this->expect_info(crl_only.check({_leaf, _ca}), RevocationStatus::revoked, RevocationSource::crl);
    this->expect_info(crl_only.check({_leaf, _ca}), RevocationStatus::revoked, RevocationSource::crl);
    EXPECT_EQ(_crl_fetches, 1);
}

TEST_F(TestRevocationChecker, failures_are_not_cached)
{
    _ocsp_up = false;
    _crl_up = false;
    EXPECT_FALSE(this->check().has_value());

    _ocsp_up = true;
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);
    EXPECT_EQ(_ocsp_fetches, 2);
}

TEST_F(TestRevocationChecker, max_cache_age_caps_reuse)
{
    _checker.set_max_cache_age(Duration(0));

    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_ocsp_fetches, 2);
}

TEST_F(TestRevocationChecker, chain_needs_issuer)
{
    auto info = _checker.check({_leaf});

    ASSERT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(info.error().message, "revocation check needs the certificate and its issuer");
}

} // namespace test
