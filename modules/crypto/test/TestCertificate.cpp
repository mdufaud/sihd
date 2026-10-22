#include <limits>

#include <gtest/gtest.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

#include "crypto_test_helpers.hpp"

using namespace sihd::util;

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

class TestCertificate: public ::testing::Test
{
    protected:
        TestCertificate() { LoggerManager::stream(); }
        ~TestCertificate() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            _key = rsa_key();
            ASSERT_TRUE(_key);
        }

        PrivateKey _key;
};

TEST_F(TestCertificate, generate_self_signed)
{
    Certificate cert;
    EXPECT_FALSE(cert);
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));
    EXPECT_TRUE(cert);
    EXPECT_EQ(cert.subject_common_name(), "test.local");
    EXPECT_EQ(cert.issuer_common_name(), "test.local");
    EXPECT_FALSE(cert.serial_hex().empty());
    EXPECT_FALSE(cert.is_ca());
}

TEST_F(TestCertificate, generate_self_signed_with_options)
{
    CertOptions opts;
    opts.common_name = "ca.example.com";
    opts.organization = "Example Corp";
    opts.organizational_unit = "IT";
    opts.country = "FR";
    opts.state = "Ile-de-France";
    opts.locality = "Paris";
    opts.email = "admin@example.com";
    opts.subject_alt_names = {"DNS:example.com", "DNS:www.example.com", "IP:127.0.0.1"};
    opts.days = 30;
    opts.serial = 0x1234;
    opts.is_ca = true;

    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, opts));

    EXPECT_EQ(cert.subject_common_name(), "ca.example.com");
    EXPECT_EQ(cert.issuer_common_name(), "ca.example.com");
    EXPECT_EQ(cert.serial_hex(), "1234");
    EXPECT_TRUE(cert.is_ca());

    std::vector<std::string> sans = cert.subject_alt_names();
    EXPECT_EQ(sans.size(), 3u);
    EXPECT_EQ(sans[0], "DNS:example.com");
    EXPECT_EQ(sans[1], "DNS:www.example.com");
    EXPECT_EQ(sans[2], "IP:127.0.0.1");

    int64_t validity_secs = (cert.not_after() - cert.not_before()).seconds();
    EXPECT_EQ(validity_secs, 30 * 24 * 60 * 60);
}

TEST_F(TestCertificate, generate_self_signed_invalid_san)
{
    CertOptions opts;
    opts.subject_alt_names = {"not-a-valid-san"};

    Certificate cert;
    auto res = cert.generate_self_signed(_key, opts);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error().code, ErrorCode::invalid_argument);
    EXPECT_FALSE(cert);
}

TEST_F(TestCertificate, generate_self_signed_rejects_alt_name_injection)
{
    for (const char *entry : {"DNS:a.com,DNS:*.victim.com",
                              "DNS:a.com\nDNS:b.com",
                              "otherName:1.2.3.4;UTF8:x",
                              "dirName:dir_sect",
                              "RID:1.2.3.4"})
    {
        CertOptions opts;
        opts.common_name = "a.com";
        opts.subject_alt_names = {entry};

        Certificate cert;
        auto res = cert.generate_self_signed(_key, opts);
        ASSERT_FALSE(res) << entry;
        EXPECT_EQ(res.error().code, ErrorCode::invalid_argument) << entry;
        EXPECT_EQ(res.error().message, std::string("invalid subject alt name: ") + entry);
        EXPECT_FALSE(cert);
    }

    CertOptions opted_out;
    opted_out.common_name = "a.com";
    opted_out.any_alt_name_kind = true;

    opted_out.subject_alt_names = {"RID:1.2.3.4"};
    Certificate registered;
    EXPECT_TRUE(registered.generate_self_signed(_key, opted_out).has_value());

    opted_out.subject_alt_names = {"RID:1.2.3.4,DNS:*.victim.com"};
    Certificate smuggled;
    auto refused = smuggled.generate_self_signed(_key, opted_out);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().message, "invalid subject alt name: RID:1.2.3.4,DNS:*.victim.com");
}

TEST_F(TestCertificate, generate_self_signed_rejects_bad_validity)
{
    for (const int days : {0, -1})
    {
        Certificate cert;
        auto res = cert.generate_self_signed(_key, "localhost", days);
        ASSERT_FALSE(res) << days;
        EXPECT_EQ(res.error().code, ErrorCode::invalid_argument);
        EXPECT_EQ(res.error().message, "certificate validity must be at least 1 day");
    }

    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "localhost", 36500));
}

TEST_F(TestCertificate, generate_self_signed_fails_on_unencodable_validity)
{
    Certificate cert;
    auto res = cert.generate_self_signed(_key, "localhost", std::numeric_limits<int>::max());
    ASSERT_FALSE(res);
    EXPECT_FALSE(cert);
}

TEST_F(TestCertificate, generate_self_signed_rejects_unencodable_subject)
{
    CertOptions opts;
    opts.common_name = std::string(70, 'a');

    Certificate cert;
    auto res = cert.generate_self_signed(_key, opts);
    ASSERT_FALSE(res);
    EXPECT_FALSE(cert);

    opts.common_name = std::string("good.com\0.evil.com", 18);
    res = cert.generate_self_signed(_key, opts);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(res.error().message, "subject field CN contains a NUL byte");
    EXPECT_FALSE(cert);
}

TEST_F(TestCertificate, verify_requires_matching_issuer)
{
    Certificate first;
    Certificate second;
    ASSERT_TRUE(first.generate_self_signed(_key, "first.example"));
    ASSERT_TRUE(second.generate_self_signed(_key, "second.example"));

    EXPECT_TRUE(first.verify(first));
    EXPECT_FALSE(first.verify(second));
}

TEST_F(TestCertificate, pem_roundtrip)
{
    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));

    std::string pem = cert.to_pem_string();
    EXPECT_FALSE(pem.empty());

    Certificate cert2;
    EXPECT_TRUE(cert2.load_pem_string(pem).has_value());
    EXPECT_TRUE(cert2);

    EXPECT_EQ(cert2.to_pem_string(), pem);
}

TEST_F(TestCertificate, pem_file_roundtrip)
{
    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine(dir.path(), "cert.pem");

    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));
    EXPECT_TRUE(cert.save_pem(path).has_value());

    Certificate cert2;
    EXPECT_TRUE(cert2.load_pem(path).has_value());
    EXPECT_EQ(cert2.to_pem_string(), cert.to_pem_string());
}

TEST_F(TestCertificate, verify_self_signed)
{
    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));
    EXPECT_TRUE(cert.verify(cert));
}

TEST_F(TestCertificate, verify_wrong_ca)
{
    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));

    PrivateKey other_key = other_rsa_key();
    ASSERT_TRUE(other_key);
    Certificate other_cert;
    EXPECT_TRUE(other_cert.generate_self_signed(other_key, "other.local"));

    EXPECT_FALSE(cert.verify(other_cert));
}

TEST_F(TestCertificate, copy)
{
    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));

    Certificate copy = cert;
    EXPECT_TRUE(copy);
    EXPECT_EQ(copy.native(), cert.native());
}

} // namespace test
