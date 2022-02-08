#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/SharedMemory.hpp>

#include <semaphore.h>
#define BUF_SIZE 2
struct shmbuf
{
    sem_t   sem1;            /* POSIX unnamed semaphore */
    sem_t   sem2;            /* POSIX unnamed semaphore */
    int     data[BUF_SIZE];   /* Data being transferred */
};

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    using namespace sihd::util;
    class TestSharedMemory: public ::testing::Test
    {
        protected:
            TestSharedMemory()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "util",
                    "sharedmemory"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestSharedMemory()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestSharedMemory, test_sharedmemory)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");
        int pid = fork();
        ASSERT_NE(pid, -1);
        if (pid != 0)
        {
            SIHD_LOG(debug, "--- begin of parent ---");
            usleep(1E5);
            SIHD_LOG(debug, "--- parent attaching ---");
            // no attach_read_only with semaphores - it segfaults
            SharedMemory mem;
            ASSERT_TRUE(mem.attach("/id", sizeof(shmbuf)));
            if (mem.data())
            {
                SIHD_LOG(debug, "--- parent attached ---");
                struct shmbuf *shmp = (struct shmbuf *)mem.data();
                EXPECT_NE(sem_wait(&shmp->sem1), -1);
                SIHD_LOG(debug, "data[0] -> " << shmp->data[0]);
                SIHD_LOG(debug, "data[1] -> " << shmp->data[1]);
                EXPECT_EQ(shmp->data[0], 42);
                EXPECT_EQ(shmp->data[1], 24);
                EXPECT_NE(sem_post(&shmp->sem2), -1);
            }
            SIHD_LOG(debug, "--- end of parent ---");
        }
        else if (pid == 0)
        {
            SIHD_LOG(debug, "--- begin of child ---");
            SharedMemory mem;
            ASSERT_TRUE(mem.create("/id", sizeof(shmbuf)));
            if (mem.data())
            {
                SIHD_LOG(debug, "--- child created shared memory ---");
                struct shmbuf *shmp = (struct shmbuf *)mem.data();
                ASSERT_NE(sem_init(&shmp->sem1, 1, 0), -1);
                ASSERT_NE(sem_init(&shmp->sem2, 1, 0), -1);
                shmp->data[0] = 42;
                shmp->data[1] = 24;
                EXPECT_NE(sem_post(&shmp->sem1), -1);
                SIHD_LOG(debug, "--- child wrote data - waiting for parent to read it ---");
                EXPECT_NE(sem_wait(&shmp->sem2), -1);
            }
            SIHD_LOG(debug, "--- end of child ---");
            exit(0);
        }
    }

}