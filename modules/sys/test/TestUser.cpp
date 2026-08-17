#include <gtest/gtest.h>

#include <sihd/sys/user.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestUser: public ::testing::Test
{
    protected:
        TestUser() { sihd::util::LoggerManager::stream(); }

        virtual ~TestUser() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!user::supported)
            {
                GTEST_SKIP() << "user identity not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestUser, test_user_name)
{
    const auto name = user::name();
    ASSERT_TRUE(name.has_value());
    EXPECT_FALSE(name->empty());
    SIHD_LOG(info, "current user: {}", *name);
}

TEST_F(TestUser, test_user_id_valid)
{
    const user::UserId invalid;
    EXPECT_FALSE(invalid.valid());
    EXPECT_EQ(invalid.to_string(), "");

    const user::UserId current = user::effective_user();
    EXPECT_TRUE(current.valid());
    EXPECT_FALSE(current.to_string().empty());
    SIHD_LOG(info, "current user id: {}", current.to_string());

    EXPECT_TRUE(user::effective_group().valid());
    EXPECT_TRUE(user::real_user().valid());
    EXPECT_TRUE(user::real_group().valid());

    // an invalid id never equals a real one
    EXPECT_NE(current, invalid);
    EXPECT_EQ(current, user::effective_user());
}

TEST_F(TestUser, test_user_id_string_roundtrip)
{
    const user::UserId current = user::effective_user();
    const auto parsed = user::UserId::from_string(current.to_string());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, current);

    const user::GroupId group = user::effective_group();
    const auto parsed_group = user::GroupId::from_string(group.to_string());
    ASSERT_TRUE(parsed_group.has_value());
    EXPECT_EQ(*parsed_group, group);

    EXPECT_FALSE(user::UserId::from_string("").has_value());
    EXPECT_FALSE(user::UserId::from_string("not an id").has_value());
}

TEST_F(TestUser, test_user_lookup_roundtrip)
{
    const auto name = user::name();
    ASSERT_TRUE(name.has_value());

    const auto id = user::user_id_of(*name);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, user::effective_user());

    const auto resolved = user::name_of(*id);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, *name);

    EXPECT_FALSE(user::user_id_of("sihd_no_such_user").has_value());
    EXPECT_FALSE(user::name_of(user::UserId {}).has_value());
}

TEST_F(TestUser, test_user_group_lookup)
{
    // a group present on every installation of the platform
#if defined(__SIHD_WINDOWS__)
    constexpr const char *known_group = "Everyone";
#else
    constexpr const char *known_group = "root";
#endif

    const auto id = user::group_id_of(known_group);
    ASSERT_TRUE(id.has_value());
    EXPECT_TRUE(id->valid());

    const auto resolved = user::name_of(*id);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, known_group);

    EXPECT_FALSE(user::group_id_of("sihd_no_such_group").has_value());
    EXPECT_FALSE(user::name_of(user::GroupId {}).has_value());
}

TEST_F(TestUser, test_user_primary_group)
{
    const user::GroupId group = user::effective_group();
    // the name the OS reports is not guaranteed to be resolvable back: windows names the
    // default primary group "None", which LookupAccountName does not map (ERROR_NONE_MAPPED)
    const auto group_name = user::name_of(group);
    ASSERT_TRUE(group_name.has_value());
    EXPECT_FALSE(group_name->empty());
    SIHD_LOG(info, "primary group: {}", *group_name);

    const auto name = user::name();
    ASSERT_TRUE(name.has_value());
    const auto primary = user::primary_group_of(*name);
    ASSERT_TRUE(primary.has_value());
    EXPECT_EQ(*primary, group);

    // the id overload resolves the same group as the name one
    const auto primary_by_id = user::primary_group_of(user::effective_user());
    ASSERT_TRUE(primary_by_id.has_value());
    EXPECT_EQ(*primary_by_id, group);

    EXPECT_FALSE(user::primary_group_of("sihd_no_such_user").has_value());
    EXPECT_FALSE(user::primary_group_of(user::UserId {}).has_value());
}

TEST_F(TestUser, test_user_groups)
{
    // a minimal environment may expose no supplementary group: only the content of what is
    // returned can be asserted on
    // note duplicates are legitimate: in a user namespace every unmapped group is reported as the
    // same overflow gid
    const auto groups = user::groups();
    for (const user::GroupId & group : groups)
    {
        EXPECT_TRUE(group.valid());
        SIHD_LOG(info, "group: {}", group.to_string());
        // every returned id is a usable one: it round-trips through the public string form
        const auto parsed = user::GroupId::from_string(group.to_string());
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, group);
    }
}

// native() has a different type per platform, so the two cases cannot share a body
#if defined(__SIHD_WINDOWS__)

TEST_F(TestUser, test_user_is_root)
{
    // windows elevation is a token property, not a uid: only that it can be queried is asserted
    SIHD_LOG(info, "is_root: {}", user::is_root());
}

TEST_F(TestUser, test_user_drop_privileges_unsupported)
{
    // windows cannot permanently change the account of a running process
    EXPECT_FALSE(user::drop_privileges(user::effective_user(), user::effective_group()));
    EXPECT_FALSE(user::drop_privileges("sihd_no_such_user"));
}

#else

TEST_F(TestUser, test_user_is_root)
{
    EXPECT_EQ(user::is_root(), user::effective_user().native() == 0);
}

TEST_F(TestUser, test_user_drop_privileges_denied)
{
    // an invalid id is never dropped to
    EXPECT_FALSE(user::drop_privileges(user::UserId {}, user::GroupId {}));
    EXPECT_FALSE(user::drop_privileges("sihd_no_such_user"));

    const user::UserId before_user = user::effective_user();
    const user::GroupId before_group = user::effective_group();

    if (before_user.native() == 0)
        GTEST_SKIP() << "running as root: dropping is permitted and irreversible in-process";

    // an unprivileged process cannot become another user
    const user::UserId other_user = user::UserId::from_native(before_user.native() + 1);
    const user::GroupId other_group = user::GroupId::from_native(before_group.native() + 1);
    EXPECT_FALSE(user::drop_privileges(other_user, other_group));

    // the failed attempt must not have changed the identity
    EXPECT_EQ(user::effective_user(), before_user);
    EXPECT_EQ(user::effective_group(), before_group);
}

#endif
} // namespace test
