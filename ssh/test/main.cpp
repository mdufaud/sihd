#include <gtest/gtest.h>
#include <sihd/ssh/utils.hpp>

// Global test environment to verify SSH utils are properly finalized
class SshTestEnvironment: public ::testing::Environment
{
    public:
        ~SshTestEnvironment() override = default;

        void SetUp() override
        {
            // Ensure SSH is not initialized at the start
            if (sihd::ssh::utils::is_initialized())
            {
                ADD_FAILURE() << "SSH utils already initialized at test start";
            }
        }

        void TearDown() override
        {
            // Verify all SSH init/finalize calls are balanced
            if (sihd::ssh::utils::is_initialized())
            {
                ADD_FAILURE()
                    << "SSH utils still initialized after all tests - init/finalize calls not balanced";
                // Cleanup for next run
                while (sihd::ssh::utils::is_initialized())
                {
                    sihd::ssh::utils::finalize();
                }
            }
        }
};

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new SshTestEnvironment);
    return RUN_ALL_TESTS();
}