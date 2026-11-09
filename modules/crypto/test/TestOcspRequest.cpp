#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/OcspRequest.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/util/Logger.hpp>

#include "crypto_test_helpers.hpp"
#include <openssl/ocsp.h>

using namespace sihd::util;

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

namespace
{

struct OcspRequestFree
{
        void operator()(OCSP_REQUEST *request) const { OCSP_REQUEST_free(request); }
};
struct OcspCertIdFree
{
        void operator()(OCSP_CERTID *id) const { OCSP_CERTID_free(id); }
};

} // namespace

class TestOcspRequest: public ::testing::Test
{
    protected:
        TestOcspRequest() { LoggerManager::stream(); }
        ~TestOcspRequest() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            _ca = make_ca(rsa_key(), "ca.example");
            _leaf = this->issue("leaf.example");
            ASSERT_TRUE(_ca);
            ASSERT_TRUE(_leaf);

            ASSERT_TRUE(_request.build(_leaf, _ca));
            auto der = _request.to_der();
            ASSERT_TRUE(der);
            _request_der = *der;
        }

        // issued by the CA with other_rsa_key(), the key a delegated responder signs with
        Certificate issue(const std::string & name) const
        {
            CertOptions opts;
            opts.common_name = name;
            return make_leaf(_ca, rsa_key(), other_rsa_key(), opts);
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
    std::unique_ptr<OCSP_REQUEST, OcspRequestFree> decoded(
        d2i_OCSP_REQUEST(nullptr, &in, static_cast<long>(_request_der.size())));
    std::unique_ptr<OCSP_CERTID, OcspCertIdFree> expected(
        OCSP_cert_to_id(nullptr, static_cast<X509 *>(_leaf.native()), static_cast<X509 *>(_ca.native())));
    ASSERT_NE(decoded, nullptr);

    EXPECT_EQ(OCSP_request_onereq_count(decoded.get()), 1);
    EXPECT_EQ(OCSP_id_cmp(OCSP_onereq_get0_id(OCSP_request_onereq_get0(decoded.get(), 0)), expected.get()), 0);
    EXPECT_GE(OCSP_REQUEST_get_ext_by_NID(decoded.get(), NID_id_pkix_OCSP_Nonce, -1), 0);
}

TEST_F(TestOcspRequest, empty_request_refused)
{
    OcspRequest empty;
    EXPECT_FALSE(empty);

    auto der = empty.to_der();
    ASSERT_FALSE(der);
    EXPECT_EQ(der.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(der.error().message, "no ocsp request built");

    this->expect_error(empty.verify_response(_request_der.data(), _request_der.size(), {_leaf, _ca}),
                       ErrorCode::not_initialized,
                       "no ocsp request built");

    auto built = empty.build(_leaf, Certificate());
    ASSERT_FALSE(built);
    EXPECT_EQ(built.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(built.error().message, "cannot build an ocsp request without certificate and issuer");
}

TEST_F(TestOcspRequest, refused_build_keeps_the_built_request)
{
    auto built = _request.build(_leaf, Certificate());

    ASSERT_FALSE(built);
    ASSERT_TRUE(_request);
    auto der = _request.to_der();
    ASSERT_TRUE(der.has_value());
    EXPECT_EQ(*der, _request_der);
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
    Certificate responder = this->issue("ocsp.example");
    add_ocsp_signing_usage(responder, rsa_key());

    auto info = this->verify(ocsp_respond(_request_der, responder, other_rsa_key()));

    ASSERT_TRUE(info.has_value()) << info.error().message;
    EXPECT_EQ(info->status, RevocationStatus::good);
}

TEST_F(TestOcspRequest, verify_rejects_responder_without_ocsp_signing)
{
    const Certificate sibling = this->issue("not-a-responder.example");

    this->expect_error(this->verify(ocsp_respond(_request_der, sibling, other_rsa_key())),
                       ErrorCode::invalid_argument,
                       "ocsp response is not signed by a trusted responder: root ca not trusted");
}

TEST_F(TestOcspRequest, verify_rejects_foreign_responder)
{
    const Certificate stranger = make_ca(other_rsa_key(), "stranger.example");

    this->expect_error(this->verify(ocsp_respond(_request_der, stranger, other_rsa_key())),
                       ErrorCode::invalid_argument,
                       "ocsp response is not signed by a trusted responder: certificate verify error");
}

TEST_F(TestOcspRequest, verify_rejects_foreign_nonce)
{
    OcspAnswer foreign;
    foreign.echo_nonce = false;
    foreign.foreign_nonce = true;

    this->expect_error(this->answer(foreign),
                       ErrorCode::invalid_argument,
                       "ocsp response nonce does not match the request");
}

TEST_F(TestOcspRequest, verify_accepts_absent_nonce)
{
    OcspAnswer absent;
    absent.echo_nonce = false;

    auto info = this->answer(absent);

    ASSERT_TRUE(info.has_value()) << info.error().message;
    EXPECT_EQ(info->status, RevocationStatus::good);
}

TEST_F(TestOcspRequest, verify_rejects_expired_response)
{
    OcspAnswer expired;
    expired.this_update_offset = -7200;
    expired.next_update_offset = -3600;

    this->expect_error(this->answer(expired),
                       ErrorCode::invalid_argument,
                       "ocsp response is outside its validity period");
}

TEST_F(TestOcspRequest, verify_rejects_future_response)
{
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
    const Certificate other = this->issue("other.example");
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

    auto res = this->verify(garbage);

    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ErrorCode::invalid_argument);
    // the tail is the OpenSSL error text
    EXPECT_TRUE(res.error().message.starts_with("read ocsp response: ")) << res.error().message;
}

TEST_F(TestOcspRequest, verify_needs_trust_anchor)
{
    auto response = ocsp_respond(_request_der, _ca, rsa_key());

    this->expect_error(_request.verify_response(response.data(), response.size(), {}),
                       ErrorCode::invalid_argument,
                       "ocsp verification needs a trust anchor");
}

} // namespace test
