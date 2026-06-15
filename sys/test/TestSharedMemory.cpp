#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/term.hpp>

#include "test_helper.hpp"

struct shmbuf
{
        int data[2];
};

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::sys;

class TestSharedMemory: public ::testing::Test
{
    protected:
        TestSharedMemory() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSharedMemory() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSharedMemory, test_sharedmemory)
{
    SharedMemory first;
    ASSERT_TRUE(first.create("/id", sizeof(shmbuf)));
    // testing move
    SharedMemory mem(std::move(first));
    ASSERT_NE(mem.data(), nullptr);
    EXPECT_TRUE(mem.creator());

    struct shmbuf *shmp = (struct shmbuf *)mem.data();
    shmp->data[0] = 42;
    shmp->data[1] = 24;

    // another process attaches, reads our writes and writes back its own
    Process proc({helper_path(), "shm", "/id"});
    proc.execute();
    proc.wait_any();

    EXPECT_TRUE(proc.return_code() & (1 << 1));
    EXPECT_TRUE(proc.return_code() & (1 << 2));

    EXPECT_EQ(shmp->data[0], 7);
    EXPECT_EQ(shmp->data[1], 8);

    EXPECT_TRUE(mem.clear());
}

} // namespace test
