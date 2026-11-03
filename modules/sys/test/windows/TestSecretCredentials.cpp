#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/SecretStore.hpp>
#include <sihd/sys/Uuid.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestSecretCredentials: public ::testing::Test
{
    protected:
        TestSecretCredentials() { LoggerManager::stream(); }

        virtual ~TestSecretCredentials() { LoggerManager::clear_loggers(); }

        virtual void TearDown() { (void)_store.remove(_service, "account"); }

        SecretStore _store;
        const std::string _service = "sihd-test-" + Uuid().str();
};

TEST_F(TestSecretCredentials, test_store_lookup_remove)
{
    auto stored = _store.store(_service, "account", "first");
    ASSERT_TRUE(stored.has_value()) << stored.error().message;
    EXPECT_EQ(_store.lookup(_service, "account").value_or("unreadable"), "first");

    stored = _store.store(_service, "account", std::string("sec\0ond", 7));
    ASSERT_TRUE(stored.has_value()) << stored.error().message;
    EXPECT_EQ(_store.lookup(_service, "account").value_or("unreadable"), std::string("sec\0ond", 7));

    auto removed = _store.remove(_service, "account");
    ASSERT_TRUE(removed.has_value()) << removed.error().message;
}

TEST_F(TestSecretCredentials, test_not_found)
{
    auto looked_up = _store.lookup(_service, "account");
    auto removed = _store.remove(_service, "account");

    const std::string message = "no secret for account 'account' of service '" + _service + "'";
    ASSERT_FALSE(looked_up.has_value());
    EXPECT_EQ(looked_up.error().code, ErrorCode::not_found);
    EXPECT_EQ(looked_up.error().message, message);
    ASSERT_FALSE(removed.has_value());
    EXPECT_EQ(removed.error().code, ErrorCode::not_found);
    EXPECT_EQ(removed.error().message, message);
}

} // namespace test
