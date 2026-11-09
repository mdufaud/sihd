#include <gtest/gtest.h>

#include <sihd/ssh/SshKey.hpp>
#include <sihd/util/Logger.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;
using namespace sihd::util;

class TestSshKey: public ::testing::Test
{
    protected:
        TestSshKey() { LoggerManager::stream(); }

        virtual ~TestSshKey() { LoggerManager::clear_loggers(); }
};

TEST_F(TestSshKey, test_sshkey_is_equal)
{
    SshKey client_private;
    SshKey client_private_copy;
    SshKey host_private;
    ASSERT_TRUE(client_private.import_privkey_file(CLIENT_KEY_PATH).has_value());
    ASSERT_TRUE(client_private_copy.import_privkey_file(CLIENT_KEY_PATH).has_value());
    ASSERT_TRUE(host_private.import_privkey_file(HOST_KEY_PATH).has_value());

    EXPECT_TRUE(client_private.is_equal(client_private_copy));
    EXPECT_FALSE(client_private.is_equal(host_private));

    SshKey client_public;
    SshKey client_public_copy;
    SshKey host_public;
    ASSERT_TRUE(client_public.import_pubkey_file(CLIENT_PUBKEY_PATH).has_value());
    ASSERT_TRUE(client_public_copy.import_pubkey_file(CLIENT_PUBKEY_PATH).has_value());
    ASSERT_TRUE(host_public.import_pubkey_file("test/resources/test_host_key_pem.pub").has_value());

    EXPECT_TRUE(client_public.is_equal(client_public_copy));
    EXPECT_FALSE(client_public.is_equal(host_public));
}

} // namespace test
