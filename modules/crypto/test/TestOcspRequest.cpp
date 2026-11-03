#include <gtest/gtest.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/OcspRequest.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/util/Logger.hpp>

#include "crypto_test_helpers.hpp"

using namespace sihd::util;

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

class TestOcspRequest: public ::testing::Test
{
    protected:
        TestOcspRequest() { LoggerManager::stream(); }
        ~TestOcspRequest() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            _ca = make_ca(rsa_key(), "ca.example");
            CertOptions opts;
            opts.common_name = "leaf.example";
            _leaf = make_leaf(_ca, rsa_key(), opts);
            ASSERT_TRUE(_ca);
            ASSERT_TRUE(_leaf);
            ASSERT_TRUE(_request.build(_leaf, _ca));
            auto der = _request.to_der();
            ASSERT_TRUE(der);
            _request_der = *der;
        }

        Result<RevocationInfo> verify(const std::vector<uint8_t> & response) const
        {
            return _request.verify_response(response.data(), response.size(), {_leaf, _ca});
        }

        Result<RevocationInfo> answer(const OcspAnswer & answer) const
        {
            return this->verify(ocsp_respond(_request_der, _ca, rsa_key(), answer));
        }

        void expect_error(const Result<RevocationInfo> & res, ErrorCode code, const std::string & message) const
        {
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error().code, code);
            EXPECT_EQ(res.error().message, message);
        }

        Certificate _ca;
        Certificate _leaf;
        OcspRequest _request;
        std::vector<uint8_t> _request_der;
};

TEST_F(TestOcspRequest, to_der_carries_cert_id_and_nonce)
{
    const unsigned char *in = _request_der.data();
    OCSP_REQUEST *decoded = d2i_OCSP_REQUEST(nullptr, &in, static_cast<long>(_request_der.size()));
    ASSERT_NE(decoded, nullptr);
    OCSP_CERTID *expected = OCSP_cert_to_id(nullptr,
                                            static_cast<X509 *>(_leaf.native()),
                                            static_cast<X509 *>(_ca.native()));

    EXPECT_EQ(OCSP_request_onereq_count(decoded), 1);
    EXPECT_EQ(OCSP_id_cmp(OCSP_onereq_get0_id(OCSP_request_onereq_get0(decoded, 0)), expected), 0);
    EXPECT_GE(OCSP_REQUEST_get_ext_by_NID(decoded, NID_id_pkix_OCSP_Nonce, -1), 0);

    OCSP_CERTID_free(expected);
    OCSP_REQUEST_free(decoded);
}

TEST_F(TestOcspRequest, empty_request_refused)
{
    OcspRequest empty;
    EXPECT_FALSE(empty);
    auto der = empty.to_der();
    ASSERT_FALSE(der);
    EXPECT_EQ(der.error().code, ErrorCode::not_initialized);

    auto built = empty.build(_leaf, Certificate());
    ASSERT_FALSE(built);
    EXPECT_EQ(built.error().message, "cannot build an ocsp request without certificate and issuer");
}

TEST_F(TestOcspRequest, verify_reports_each_status)
{
    const std::vector<std::pair<int, RevocationStatus>> cases {
        {V_OCSP_CERTSTATUS_GOOD, RevocationStatus::good},
        {V_OCSP_CERTSTATUS_REVOKED, RevocationStatus::revoked},
        {V_OCSP_CERTSTATUS_UNKNOWN, RevocationStatus::unknown},
    };
    for (const auto & [ocsp_status, expected] : cases)
    {
        OcspAnswer ocsp_answer;
        ocsp_answer.status = ocsp_status;
        auto info = this->answer(ocsp_answer);

        ASSERT_TRUE(info.has_value()) << info.error().message;
        EXPECT_EQ(info->status, expected);
        EXPECT_EQ(info->source, RevocationSource::ocsp);
        EXPECT_EQ(info->next_update.seconds() - info->this_update.seconds(), 3600);
        EXPECT_EQ(info->revocation_time.seconds() != 0, expected == RevocationStatus::revoked);
    }
}

TEST_F(TestOcspRequest, verify_accepts_delegated_responder)
{
    CertOptions opts;
    opts.common_name = "ocsp.example";
    Certificate responder = make_leaf(_ca, rsa_key(), opts);
    add_ocsp_signing_usage(responder, rsa_key());

    auto info = this->verify(ocsp_respond(_request_der, responder, other_rsa_key()));

    ASSERT_TRUE(info.has_value()) << info.error().message;
    EXPECT_EQ(info->status, RevocationStatus::good);
}

TEST_F(TestOcspRequest, verify_rejects_unauthorized_signer)
{
    CertOptions opts;
    opts.common_name = "not-a-responder.example";
    Certificate sibling = make_leaf(_ca, rsa_key(), opts);
    Certificate stranger = make_ca(other_rsa_key(), "stranger.example");

    EXPECT_FALSE(this->verify(ocsp_respond(_request_der, sibling, other_rsa_key())).has_value());
    EXPECT_FALSE(this->verify(ocsp_respond(_request_der, stranger, other_rsa_key())).has_value());
}

TEST_F(TestOcspRequest, verify_checks_nonce)
{
    OcspAnswer foreign;
    foreign.echo_nonce = false;
    foreign.foreign_nonce = true;
    this->expect_error(this->answer(foreign),
                       ErrorCode::invalid_argument,
                       "ocsp response nonce does not match the request");

    OcspAnswer absent;
    absent.echo_nonce = false;
    EXPECT_TRUE(this->answer(absent).has_value());
}

TEST_F(TestOcspRequest, verify_rejects_outdated_response)
{
    OcspAnswer expired;
    expired.this_update_offset = -7200;
    expired.next_update_offset = -3600;
    this->expect_error(this->answer(expired),
                       ErrorCode::invalid_argument,
                       "ocsp response is outside its validity period");

    OcspAnswer future;
    future.this_update_offset = 3600;
    future.next_update_offset = 7200;
    this->expect_error(this->answer(future),
                       ErrorCode::invalid_argument,
                       "ocsp response is outside its validity period");
}

TEST_F(TestOcspRequest, verify_reports_responder_refusal)
{
    this->expect_error(this->verify(ocsp_refuse(OCSP_RESPONSE_STATUS_TRYLATER)),
                       ErrorCode::io_error,
                       "ocsp responder answered trylater");
}

TEST_F(TestOcspRequest, verify_rejects_other_request_answer)
{
    CertOptions opts;
    opts.common_name = "other.example";
    Certificate other = make_leaf(_ca, rsa_key(), opts);
    OcspRequest other_request;
    ASSERT_TRUE(other_request.build(other, _ca));
    auto other_der = other_request.to_der();
    ASSERT_TRUE(other_der);

    OcspAnswer no_nonce;
    no_nonce.echo_nonce = false;
    this->expect_error(this->verify(ocsp_respond(*other_der, _ca, rsa_key(), no_nonce)),
                       ErrorCode::not_found,
                       "ocsp response has no status for the certificate");
}

TEST_F(TestOcspRequest, verify_rejects_garbage)
{
    const std::vector<uint8_t> garbage {0x30, 0x03, 0x01, 0x02, 0x03};
    EXPECT_FALSE(this->verify(garbage).has_value());
    EXPECT_FALSE(_request.verify_response(garbage.data(), garbage.size(), {}).has_value());
}

} // namespace test
