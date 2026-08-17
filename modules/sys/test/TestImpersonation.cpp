#include <gtest/gtest.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include <sihd/sys/Impersonation.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestImpersonation: public ::testing::Test
{
    protected:
        TestImpersonation() { sihd::util::LoggerManager::stream(); }

        virtual ~TestImpersonation() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestImpersonation, test_impersonation_credentials)
{
    Impersonation impersonation;
    EXPECT_FALSE(impersonation.impersonating());

    if constexpr (!Impersonation::supports_credentials)
    {
        // unix authenticates through PAM, not through the identity switch
        EXPECT_FALSE(impersonation.impersonate_with_credentials("user", "password"));
        EXPECT_FALSE(impersonation.impersonating());
    }
    else
    {
        // impersonating an account that cannot be logged on must leave the thread untouched.
        // note this asserts the outcome, not which win32 call rejected it: under wine LogonUser
        // is a stub returning success and a fake token, and ImpersonateLoggedOnUser is what
        // fails (ERROR_INVALID_HANDLE) - which is exactly the cleanup path being covered here
        EXPECT_FALSE(impersonation.impersonate_with_credentials("sihd_no_such_user", "sihd_no_such_password"));
        EXPECT_FALSE(impersonation.impersonating());
    }

    // reverting when not impersonating reports that nothing was undone
    EXPECT_FALSE(impersonation.revert());
    EXPECT_TRUE(user::effective_user().valid());
}

TEST_F(TestImpersonation, test_impersonation_privileged_unsupported)
{
    if constexpr (Impersonation::supports_privileged)
    {
        GTEST_SKIP() << "privileged impersonation is supported on this platform";
    }
    else
    {
        Impersonation impersonation;
        EXPECT_FALSE(impersonation.impersonate_as(user::effective_user(), user::effective_group()));
        EXPECT_FALSE(impersonation.impersonating());
    }
}

TEST_F(TestImpersonation, test_impersonation_invalid_id)
{
    if constexpr (!Impersonation::supports_privileged)
    {
        GTEST_SKIP() << "privileged impersonation not supported on this platform";
    }
    else
    {
        Impersonation impersonation;
        EXPECT_FALSE(impersonation.impersonate_as(user::UserId {}, user::GroupId {}));
        EXPECT_FALSE(impersonation.impersonating());
    }
}

// native() is unix only, so this cannot share a body with the windows build
#if !defined(__SIHD_WINDOWS__)

TEST_F(TestImpersonation, test_impersonation_self_roundtrip)
{
    if constexpr (!Impersonation::supports_privileged)
    {
        GTEST_SKIP() << "privileged impersonation not supported on this platform";
    }
    else
    {
        const user::UserId before = user::effective_user();
        const user::GroupId before_group = user::effective_group();
        const user::UserId real_before = user::real_user();

        // switching to the identity the thread already holds needs no privilege
        Impersonation impersonation;
        ASSERT_TRUE(impersonation.impersonate_as(before, before_group));
        EXPECT_TRUE(impersonation.impersonating());
        EXPECT_EQ(user::effective_user(), before);

        ASSERT_TRUE(impersonation.revert());
        EXPECT_FALSE(impersonation.impersonating());
        EXPECT_EQ(user::effective_user(), before);
        EXPECT_EQ(user::effective_group(), before_group);
        // an impersonation never touches the real identity of the process
        EXPECT_EQ(user::real_user(), real_before);
    }
}

TEST_F(TestImpersonation, test_impersonation_other_identity)
{
    if constexpr (!Impersonation::supports_privileged)
    {
        GTEST_SKIP() << "privileged impersonation not supported on this platform";
    }
    else
    {
        const user::UserId before = user::effective_user();
        const user::GroupId before_group = user::effective_group();
        const user::UserId real_before = user::real_user();

        // 'nobody' exists on every usual installation and is mapped in a user namespace only when
        // the namespace has a uid range - fall back to the next uid, which needs real root
        const auto nobody = user::user_id_of("nobody");
        const user::UserId target = nobody.value_or(user::UserId::from_native(before.native() + 1));
        const auto nobody_group = user::primary_group_of(target);
        const user::GroupId target_group
            = nobody_group.value_or(user::GroupId::from_native(before_group.native() + 1));

        Impersonation impersonation;
        const bool switched = impersonation.impersonate_as(target, target_group);

        if (!switched)
        {
            // unprivileged: the attempt must leave the thread exactly as it was
            EXPECT_FALSE(impersonation.impersonating());
            EXPECT_EQ(user::effective_user(), before);
            EXPECT_EQ(user::effective_group(), before_group);
            GTEST_SKIP() << "cannot switch to uid " << target.to_string()
                         << ": needs CAP_SETUID and the target uid mapped (real root, or a user "
                            "namespace created with a uid range)";
        }

        // privileged: the thread really became someone else, and comes back
        EXPECT_TRUE(impersonation.impersonating());
        EXPECT_EQ(user::effective_user(), target);
        EXPECT_EQ(user::effective_group(), target_group);
        // only the calling thread switched: the process identity is untouched
        EXPECT_EQ(user::real_user(), real_before);

        ASSERT_TRUE(impersonation.revert());
        EXPECT_FALSE(impersonation.impersonating());
        EXPECT_EQ(user::effective_user(), before);
        EXPECT_EQ(user::effective_group(), before_group);
    }
}

TEST_F(TestImpersonation, test_impersonation_is_per_thread)
{
    if constexpr (!Impersonation::supports_privileged)
    {
        GTEST_SKIP() << "privileged impersonation not supported on this platform";
    }
    else
    {
        const user::UserId before = user::effective_user();
        const user::GroupId before_group = user::effective_group();

        // 'nobody' exists on every usual installation and is mapped in a user namespace only when
        // the namespace has a uid range - fall back to the next uid, which needs real root
        const auto nobody = user::user_id_of("nobody");
        const user::UserId target = nobody.value_or(user::UserId::from_native(before.native() + 1));
        const auto nobody_group = user::primary_group_of(target);
        const user::GroupId target_group
            = nobody_group.value_or(user::GroupId::from_native(before_group.native() + 1));

        // the switch must stay confined to the worker thread - the libc wrappers would broadcast it
        enum class State
        {
            pending,
            switched,
            failed,
        };
        State state = State::pending;
        std::mutex mutex;
        std::condition_variable cv;
        bool main_checked = false;

        std::thread worker([&] {
            Impersonation impersonation;
            if (!impersonation.impersonate_as(target, target_group))
            {
                std::lock_guard lock(mutex);
                state = State::failed;
                cv.notify_one();
                return;
            }
            EXPECT_EQ(user::effective_user(), target);
            {
                std::lock_guard lock(mutex);
                state = State::switched;
            }
            cv.notify_one();

            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return main_checked; });
            EXPECT_TRUE(impersonation.revert());
            EXPECT_EQ(user::effective_user(), before);
        });

        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return state != State::pending; });
        // sampled while the worker thread is switched
        const bool identity_intact = user::effective_user() == before;
        main_checked = true;
        lock.unlock();
        cv.notify_one();
        worker.join();

        if (state == State::failed)
        {
            GTEST_SKIP() << "cannot switch to uid " << target.to_string()
                         << ": needs CAP_SETUID and the target uid mapped (real root, or a user "
                            "namespace created with a uid range)";
        }
        EXPECT_TRUE(identity_intact);
    }
}

TEST_F(TestImpersonation, test_impersonation_destructor_reverts)
{
    if constexpr (!Impersonation::supports_privileged)
    {
        GTEST_SKIP() << "privileged impersonation not supported on this platform";
    }
    else
    {
        const user::UserId before = user::effective_user();
        {
            Impersonation impersonation;
            ASSERT_TRUE(impersonation.impersonate_as(before, user::effective_group()));
            EXPECT_TRUE(impersonation.impersonating());
        }
        // leaving the scope restores the identity without an explicit revert
        EXPECT_EQ(user::effective_user(), before);
    }
}

#endif
} // namespace test
