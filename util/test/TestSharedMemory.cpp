#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SharedMemory.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <semaphore.h>
#define BUF_SIZE 2
struct shmbuf
{
        sem_t sem1;         /* POSIX unnamed semaphore */
        sem_t sem2;         /* POSIX unnamed semaphore */
        int data[BUF_SIZE]; /* Data being transferred */
};

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
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
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");
    int pid = fork();
    ASSERT_NE(pid, -1);
    if (pid != 0)
    {
        SIHD_LOG(debug, "--- begin of parent ---");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        SIHD_LOG(debug, "--- parent attaching ---");
        // no attach_read_only with semaphores - it segfaults
        SharedMemory mem;
        ASSERT_TRUE(mem.attach("/id", sizeof(shmbuf)));
        ASSERT_NE(mem.data(), nullptr);
        SIHD_LOG(debug, "--- parent attached ---");

        SIHD_LOG(debug, "--- parent reading shared memory ---");
        struct shmbuf *shmp = (struct shmbuf *)mem.data();
        EXPECT_NE(sem_wait(&shmp->sem1), -1);
        SIHD_LOG(debug, "[parent] data[0] -> {}", shmp->data[0]);
        SIHD_LOG(debug, "[parent] data[1] -> {}", shmp->data[1]);
        EXPECT_EQ(shmp->data[0], 42);
        EXPECT_EQ(shmp->data[1], 24);
        EXPECT_NE(sem_post(&shmp->sem2), -1);

        SIHD_LOG(debug, "--- end of parent ---");
    }
    else if (pid == 0)
    {
        SIHD_LOG(debug, "--- begin of child ---");
        SharedMemory first;
        ASSERT_TRUE(first.create("/id", sizeof(shmbuf)));
        // testing move
        SharedMemory mem(std::move(first));
        ASSERT_NE(mem.data(), nullptr);

        SIHD_LOG(debug, "--- child created shared memory ---");
        struct shmbuf *shmp = (struct shmbuf *)mem.data();
        ASSERT_NE(sem_init(&shmp->sem1, 1, 0), -1);
        ASSERT_NE(sem_init(&shmp->sem2, 1, 0), -1);
        SIHD_LOG(debug, "--- child writing data ---");
        shmp->data[0] = 42;
        shmp->data[1] = 24;
        EXPECT_NE(sem_post(&shmp->sem1), -1);

        SIHD_LOG(debug, "--- child wrote data - waiting for parent to read it ---");
        EXPECT_NE(sem_wait(&shmp->sem2), -1);

        SIHD_LOG(debug, "--- end of child ---");
        exit(0);
    }
}

} // namespace test
