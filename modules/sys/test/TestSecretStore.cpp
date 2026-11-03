#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/SecretStore.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestSecretStore: public ::testing::Test
{
    protected:
        TestSecretStore() { LoggerManager::stream(); }

        virtual ~TestSecretStore() { LoggerManager::clear_loggers(); }

        template <typename T>
        static void expect_invalid(const Result<T> & result, const std::string & message)
        {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code, ErrorCode::invalid_argument);
            EXPECT_EQ(result.error().message, message);
        }

        SecretStore _store;
};

TEST_F(TestSecretStore, test_empty_key)
{
    const std::string message = "a secret needs a service and an account";

    expect_invalid(_store.store("", "account", "secret"), message);
    expect_invalid(_store.store("service", "", "secret"), message);
    expect_invalid(_store.lookup("", "account"), message);
    expect_invalid(_store.remove("service", ""), message);
}

TEST_F(TestSecretStore, test_slash_in_service)
{
    const std::string message = "a secret service holds no '/': 'a/b'";

    expect_invalid(_store.store("a/b", "account", "secret"), message);
    expect_invalid(_store.lookup("a/b", "account"), message);
    expect_invalid(_store.remove("a/b", "account"), message);
}

TEST_F(TestSecretStore, test_secret_too_big)
{
    const std::string secret(SecretStore::max_secret_size + 1, 'x');

    expect_invalid(_store.store("service", "account", secret), "a secret holds at most 2560 bytes, got 2561");
}

} // namespace test
