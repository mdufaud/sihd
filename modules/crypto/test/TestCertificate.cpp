#include <gtest/gtest.h>

#include <cstdio>

#include <sihd/util/Logger.hpp>

#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/Certificate.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::crypto;

class TestCertificate: public ::testing::Test
{
    protected:
        TestCertificate() { sihd::util::LoggerManager::stream(); }
        ~TestCertificate() { sihd::util::LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            ASSERT_TRUE(_key.generate_rsa(2048));
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

TEST_F(TestCertificate, pem_roundtrip)
{
    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));

    std::string pem = cert.to_pem_string();
    EXPECT_FALSE(pem.empty());

    Certificate cert2;
    EXPECT_TRUE(cert2.load_pem_string(pem));
    EXPECT_TRUE(cert2);

    EXPECT_EQ(cert2.to_pem_string(), pem);
}

TEST_F(TestCertificate, pem_file_roundtrip)
{
    std::string path = "/tmp/sihd_test_cert.pem";

    Certificate cert;
    EXPECT_TRUE(cert.generate_self_signed(_key, "test.local"));
    EXPECT_TRUE(cert.save_pem(path));

    Certificate cert2;
    EXPECT_TRUE(cert2.load_pem(path));
    EXPECT_EQ(cert2.to_pem_string(), cert.to_pem_string());

    std::remove(path.c_str());
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

    PrivateKey other_key;
    EXPECT_TRUE(other_key.generate_rsa(2048));
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
