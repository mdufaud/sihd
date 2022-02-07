#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/SharedMemory.hpp>

namespace test
{
    LOGGER;
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
        Waitable waitable;
        size_t mem_size = sizeof(int) * 2;
        int pid = fork();
        ASSERT_NE(pid, -1);
        if (pid != 0)
        {
            LOG(debug, "--- begin of parent ---");
            usleep(2E5);
            LOG(debug, "--- parent reading data ---");
            SharedMemory mem;
            ASSERT_TRUE(mem.attach("/id", mem_size));
            int *data = (int *)mem.data();
            ASSERT_NE(data, nullptr);
            LOG(debug, "data[0] -> " << data[0]);
            LOG(debug, "data[1] -> " << data[1]);
            EXPECT_EQ(data[0], 42);
            EXPECT_EQ(data[1], 24);
            usleep(4E5);
            LOG(debug, "--- end of parent ---");
        }
        else if (pid == 0)
        {
            LOG(debug, "--- begin of child ---");
            SharedMemory mem;
            ASSERT_TRUE(mem.create_read_only("/id", mem_size));
            int *data = (int *)mem.data();
            data[0] = 42;
            data[1] = 24;
            LOG(debug, "--- child wrote to data - waiting for parent ---");
            // wait for parent to read
            usleep(5E5);
            LOG(debug, "--- end of child ---");
        }
    }

}