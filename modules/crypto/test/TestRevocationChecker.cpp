#include <set>
#include <string>
#include <vector>

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
            _leaf = this->issue({"http://ocsp.test"}, {"http://crl.test/ca.crl"});
            ASSERT_TRUE(_leaf);

            _checker.set_ocsp_fetcher([this](const std::string & url, ArrUByteView request) {
                _fetched.push_back(url);
                if (_down.contains(url))
                    return unreachable();
                const std::vector<uint8_t> der(request.data(), request.data() + request.size());
                return Result<std::vector<uint8_t>>(ocsp_respond(der, _ca, rsa_key(), {.status = _ocsp_status}));
            });
            _checker.set_crl_fetcher([this](const std::string & url) { return this->fetch_crl(url); });
        }

        static Result<std::vector<uint8_t>> unreachable()
        {
            return std::unexpected(Error(ErrorCode::io_error, "unreachable"));
        }

        // the CRL of the CA revoking _leaf, signed by a stranger for the urls in _forged
        Result<std::vector<uint8_t>> fetch_crl(const std::string & url)
        {
            _fetched.push_back(url);
            if (_down.contains(url))
                return unreachable();
            return make_crl_der(_ca, _forged.contains(url) ? other_rsa_key() : rsa_key(), {.revoked = {&_leaf}});
        }

        Certificate issue(const std::vector<std::string> & ocsp_urls, const std::vector<std::string> & crl_urls) const
        {
            CertOptions opts;
            opts.common_name = "leaf.example";
            opts.ocsp_urls = ocsp_urls;
            opts.crl_urls = crl_urls;
            return make_leaf(_ca, rsa_key(), other_rsa_key(), opts);
        }

        Result<RevocationInfo> check() { return _checker.check({_leaf, _ca}); }

        void expect_info(const Result<RevocationInfo> & info, RevocationStatus status, RevocationSource source) const
        {
            ASSERT_TRUE(info.has_value()) << info.error().message;
            EXPECT_EQ(info->status, status);
            EXPECT_EQ(info->source, source);
        }

        void expect_error(const Result<RevocationInfo> & info, ErrorCode code, const std::string & message) const
        {
            ASSERT_FALSE(info.has_value());
            EXPECT_EQ(info.error().code, code);
            EXPECT_EQ(info.error().message, message);
        }

        Certificate _ca;
        Certificate _leaf;
        RevocationChecker _checker;
        std::set<std::string> _down;
        std::set<std::string> _forged;
        std::vector<std::string> _fetched;
        int _ocsp_status = V_OCSP_CERTSTATUS_GOOD;
};

TEST_F(TestRevocationChecker, ocsp_answers_first)
{
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, ocsp_failure_falls_back_to_crl)
{
    _down = {"http://ocsp.test"};

    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test", "http://crl.test/ca.crl"}));
}

TEST_F(TestRevocationChecker, next_url_answers_after_a_failed_one)
{
    _leaf = this->issue({"http://ocsp1.test", "http://ocsp2.test"}, {});
    _down = {"http://ocsp1.test"};

    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp1.test", "http://ocsp2.test"}));
}

TEST_F(TestRevocationChecker, every_source_failing_is_an_error)
{
    _leaf = this->issue({"http://ocsp1.test", "http://ocsp2.test"}, {"http://crl.test/ca.crl"});
    _down = {"http://ocsp1.test", "http://ocsp2.test", "http://crl.test/ca.crl"};

    this->expect_error(this->check(),
                       ErrorCode::io_error,
                       "ocsp http://ocsp1.test: unreachable; ocsp http://ocsp2.test: unreachable; "
                       "crl http://crl.test/ca.crl: unreachable");
}

TEST_F(TestRevocationChecker, rejected_answer_outweighs_unreachable_source)
{
    _leaf = this->issue({}, {"http://crl1.test", "http://crl2.test"});
    _forged = {"http://crl1.test"};
    _down = {"http://crl2.test"};

    this->expect_error(this->check(),
                       ErrorCode::invalid_argument,
                       "crl http://crl1.test: CRL signature does not match 'ca.example'; "
                       "crl http://crl2.test: unreachable");
}

TEST_F(TestRevocationChecker, non_http_urls_are_never_fetched)
{
    _leaf = this->issue({"file:///etc/passwd"}, {"ldap://ldap.test/ca", "gopher://crl.test/ca.crl"});

    this->expect_info(this->check(), RevocationStatus::unknown, RevocationSource::none);

    EXPECT_TRUE(_fetched.empty());
}

TEST_F(TestRevocationChecker, certificate_without_source_is_unknown)
{
    _leaf = this->issue({}, {});

    this->expect_info(this->check(), RevocationStatus::unknown, RevocationSource::none);

    EXPECT_TRUE(_fetched.empty());
}

TEST_F(TestRevocationChecker, unset_fetchers_skip_their_source)
{
    RevocationChecker bare;

    this->expect_info(bare.check({_leaf, _ca}), RevocationStatus::unknown, RevocationSource::none);
}

TEST_F(TestRevocationChecker, ocsp_answer_is_cached)
{
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, crl_is_cached)
{
    _down = {"http://ocsp.test"};

    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);
    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test", "http://crl.test/ca.crl", "http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, failures_are_not_cached)
{
    _down = {"http://ocsp.test", "http://crl.test/ca.crl"};
    EXPECT_FALSE(this->check().has_value());

    _down.clear();
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test", "http://crl.test/ca.crl", "http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, max_cache_age_caps_ocsp_reuse)
{
    _checker.set_max_cache_age(Duration(0));

    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);
    this->expect_info(this->check(), RevocationStatus::good, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test", "http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, max_cache_age_caps_crl_reuse)
{
    _leaf = this->issue({}, {"http://crl.test/ca.crl"});
    _checker.set_max_cache_age(Duration(0));

    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);
    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://crl.test/ca.crl", "http://crl.test/ca.crl"}));
}

TEST_F(TestRevocationChecker, disabled_mode_fetches_nothing)
{
    _checker.set_mode(RevocationChecker::Mode::disabled);

    this->expect_info(this->check(), RevocationStatus::unknown, RevocationSource::none);

    EXPECT_TRUE(_fetched.empty());
}

TEST_F(TestRevocationChecker, crl_mode_skips_ocsp)
{
    _checker.set_mode(RevocationChecker::Mode::crl);

    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://crl.test/ca.crl"}));
}

TEST_F(TestRevocationChecker, ocsp_mode_never_asks_crl)
{
    _checker.set_mode(RevocationChecker::Mode::ocsp);
    _down = {"http://ocsp.test"};

    this->expect_error(this->check(), ErrorCode::io_error, "ocsp http://ocsp.test: unreachable");

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, ocsp_mode_keeps_unknown_answer)
{
    _checker.set_mode(RevocationChecker::Mode::ocsp);
    _ocsp_status = V_OCSP_CERTSTATUS_UNKNOWN;

    this->expect_info(this->check(), RevocationStatus::unknown, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test"}));
}

TEST_F(TestRevocationChecker, best_effort_asks_crl_after_unknown_ocsp)
{
    _ocsp_status = V_OCSP_CERTSTATUS_UNKNOWN;

    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);
    this->expect_info(this->check(), RevocationStatus::revoked, RevocationSource::crl);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test", "http://crl.test/ca.crl"}));
}

TEST_F(TestRevocationChecker, best_effort_keeps_unknown_ocsp_when_crl_fails)
{
    _ocsp_status = V_OCSP_CERTSTATUS_UNKNOWN;
    _down = {"http://crl.test/ca.crl"};

    this->expect_info(this->check(), RevocationStatus::unknown, RevocationSource::ocsp);

    EXPECT_EQ(_fetched, (std::vector<std::string> {"http://ocsp.test", "http://crl.test/ca.crl"}));
}

TEST_F(TestRevocationChecker, chain_needs_issuer)
{
    this->expect_error(_checker.check({_leaf}),
                       ErrorCode::invalid_argument,
                       "revocation check needs the certificate and its issuer");
}

} // namespace test
