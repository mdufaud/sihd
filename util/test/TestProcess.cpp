#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/Files.hpp>
#include <experimental/filesystem>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    using namespace std::experimental;
    class TestProcess:   public ::testing::Test
    {
        protected:
            TestProcess()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestProcess()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
            std::string _base_test_dir = Files::combine({getenv("TEST_PATH"), "util_process"});
    };

    TEST_F(TestProcess, test_process_run)
    {
        Process proc{"ls", "-la"};

        auto status = proc.wait();
        EXPECT_FALSE(status.has_value());
        EXPECT_EQ(proc.return_code(), std::nullopt);
        
        EXPECT_TRUE(proc.run());
        status = proc.wait();
        EXPECT_TRUE(status.has_value());
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);

        EXPECT_TRUE(proc.end());
    }

    TEST_F(TestProcess, test_process_out)
    {
        std::string output;
        Process proc{"echo", "hello", "world"};

        proc.stdout_to([&] (const char *buffer, [[maybe_unused]] ssize_t size) { output = buffer; });
        EXPECT_EQ(output, "");
        EXPECT_TRUE(proc.run());
        EXPECT_EQ(output, "");
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world\n");
        proc.wait();

        output.clear();
        proc.clear();
        proc.stdout_to(output);
        EXPECT_TRUE(proc.run());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world\n");
        proc.wait();

        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_in)
    {
        Process proc{"cat"};
        std::string output;

        proc.stdin_from("hello world");
        proc.stdout_to(output);
        EXPECT_TRUE(proc.run());
        proc.stdin_from("1");
        proc.stdin_from("2");
        proc.stdin_from("3");
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world123");
        proc.wait();
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_in_file)
    {
        std::string test_dir = Files::combine(_base_test_dir, "in_file");
        filesystem::remove_all(test_dir);
        std::string test_file = Files::combine(test_dir, "hello.txt");
        filesystem::create_directories(Files::get_parent(test_file));

        LOG(info, "Writing file for 'cat' input: " << test_file)
        EXPECT_TRUE(Files::write_all(test_file, "hello world"));

        Process proc{"cat"};

        std::string output;
        EXPECT_TRUE(proc.stdin_from_file(test_file));
        proc.stdout_to(output);
        EXPECT_TRUE(proc.run());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world");
        proc.wait();
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_out_file)
    {
        std::string test_dir = Files::combine(_base_test_dir, "to_file");
        filesystem::remove_all(test_dir);
        std::string test_file = Files::combine(test_dir, "output.txt");
        filesystem::create_directories(Files::get_parent(test_file));
        Process proc{"echo", "hello", "world"};

        EXPECT_TRUE(proc.stdout_to_file(test_file));
        EXPECT_EQ(Files::read_all(test_file).value(), "");
        EXPECT_TRUE(proc.run());
        EXPECT_TRUE(proc.end());
        proc.wait();
        EXPECT_EQ(Files::read_all(test_file).value(), "hello world\n");
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

}
