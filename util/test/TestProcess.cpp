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
        status = proc.wait(WUNTRACED | WCONTINUED);
        EXPECT_TRUE(status.has_value());
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
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

        output.clear();
        proc.clear();
        proc.stdout_to(output);
        EXPECT_TRUE(proc.run());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world\n");

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
        EXPECT_EQ(Files::read_all(test_file).value(), "hello world\n");
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_chain)
    {
        std::string output;
        Process echo{"echo", "hello world"};
        Process wc{"wc", "-c"};
        Process cat{"cat", "-e"};

        echo.stdout_to(wc);
        wc.stdout_to(cat);
        cat.stdout_to(output);

        EXPECT_TRUE(echo.run());
        EXPECT_TRUE(wc.run());
        EXPECT_TRUE(cat.run());

        EXPECT_TRUE(echo.end());
        EXPECT_TRUE(wc.end());
        EXPECT_TRUE(cat.end());

        EXPECT_EQ(output, "12$\n");

        EXPECT_TRUE(echo.has_exited());
        EXPECT_EQ(echo.return_code(), 0);

        EXPECT_TRUE(wc.has_exited());
        EXPECT_EQ(wc.return_code(), 0);

        EXPECT_TRUE(cat.has_exited());
        EXPECT_EQ(cat.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_stderr)
    {
        std::string output;
        Process ls{"ls", "/bli/blah/blouh"};

        ls.stderr_to(output);
        EXPECT_TRUE(ls.run());
        EXPECT_TRUE(ls.end());
        EXPECT_EQ(output, "ls: cannot access '/bli/blah/blouh': No such file or directory\n");
        EXPECT_TRUE(ls.has_exited());
        EXPECT_EQ(ls.return_code(), 2);
    }

    TEST_F(TestProcess, test_process_signal_kill)
    {
        Process cat{"cat"};

        EXPECT_TRUE(cat.run());
        EXPECT_TRUE(cat.kill(SIGTERM));
        cat.wait(WUNTRACED | WCONTINUED);
        EXPECT_TRUE(cat.has_exited());
        EXPECT_TRUE(cat.has_exited_by_signal());
        TRACE("Signal exit number: " << cat.signal_exit_number().value());
        EXPECT_EQ(cat.signal_exit_number().value(), SIGTERM);
    }

    TEST_F(TestProcess, test_process_signal_stop)
    {
        Process cat{"cat"};

        EXPECT_TRUE(cat.run());
        EXPECT_TRUE(cat.kill(SIGSTOP));
        cat.wait(WUNTRACED | WCONTINUED);
        EXPECT_TRUE(cat.has_stopped_by_signal());
        TRACE("Signal stop number: " << cat.signal_stop_number().value());
        EXPECT_EQ(cat.signal_stop_number().value(), SIGSTOP);

        EXPECT_TRUE(cat.kill(SIGCONT));
        cat.wait(WUNTRACED | WCONTINUED);

        EXPECT_TRUE(cat.kill());
        cat.wait();
    }

    TEST_F(TestProcess, test_process_fun)
    {
        Process proc([]() -> int {
            std::cout << "hello world";
            std::cerr << "nope";
            return 1;
        });

        std::string out;
        std::string err;
        proc.stdout_to(out);
        proc.stderr_to(err);

        EXPECT_TRUE(proc.run());
        EXPECT_TRUE(proc.end());

        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 1);

        EXPECT_EQ(out, "hello world");
        EXPECT_EQ(err, "nope");
    }

    TEST_F(TestProcess, test_process_file)
    {
        std::string test_path = getenv("TEST_PATH");
        filesystem::path path = filesystem::path(test_path) / "util_process";
        filesystem::remove_all(path);
        filesystem::create_directories(path);
        filesystem::path stdout_path = path / "stdout.txt";
        filesystem::path stderr_path = path / "stderr.txt";

        Process proc([]() -> int {
            std::cout << "hello world";
            std::cerr << "nope";
            return 1;
        });

        std::string out;
        std::string err;
        EXPECT_TRUE(proc.stdout_to_file(stdout_path.generic_string()));
        EXPECT_TRUE(proc.stderr_to_file(stderr_path.generic_string()));

        TRACE("Redirecting stdout to: " << stdout_path);
        TRACE("Redirecting stderr to: " << stderr_path);

        EXPECT_TRUE(proc.run());
        EXPECT_TRUE(proc.end());

        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 1);

        EXPECT_EQ(Files::read_all(stdout_path.generic_string()).value(), "hello world");
        EXPECT_EQ(Files::read_all(stderr_path.generic_string()).value(), "nope");
    }

    TEST_F(TestProcess, test_process_bad_cmd)
    {
        Process proc{"ellesse", "-la"};

        EXPECT_FALSE(proc.run());
        auto status = proc.wait();
        EXPECT_FALSE(status.has_value());
        EXPECT_FALSE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), std::nullopt);
    }

}
