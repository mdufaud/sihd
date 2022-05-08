#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <filesystem>

#include <sihd/util/Worker.hpp>
#include <sihd/util/Task.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
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
            std::string _base_test_dir = FS::combine({getenv("TEST_PATH"), "util_process"});
    };

    /*
    TEST_F(TestProcess, test_process_interactive)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        std::string output;
        Process proc{"cat"};

        proc.stdout_to([] (const char *buf, [[maybe_unused]] size_t size)
        {
            SIHD_LOG(debug, buf);
        })
        .stderr_close();

        Task task([&proc] ()
        {
            proc.run();
            return true;
        });
        Worker worker(&task);
        EXPECT_TRUE(worker.start_sync_worker("proc"));
        SIHD_LOG(debug, "Kill cat process with ctrl + d")
        EXPECT_TRUE(proc.wait_process_end(time::sec(10)));
        proc.stop();
        worker.stop_worker();
    }
    */

    TEST_F(TestProcess, test_process_run)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy with valgrind";
        std::vector<std::string> res;
        std::string output;
        Process proc{"cat"};

        proc.stdin_from("hello")
        .stdout_to([&res] (const char *buf, [[maybe_unused]] size_t size)
        {
            SIHD_LOG(debug, buf);
            res.push_back(buf);
        })
        .stderr_close();

        Task task([&proc] ()
        {
            proc.run();
            return true;
        });
        Worker worker(&task);
        worker.start_sync_worker("proc");
        usleep(1000);
        EXPECT_FALSE(res.empty());
        if (!res.empty())
        {
            EXPECT_EQ(res.back(), "hello");
            proc.stdin_from("world");
            usleep(1000);
            EXPECT_EQ(res.back(), "world");
            proc.stdin_from("how");
            usleep(1000);
            EXPECT_EQ(res.back(), "how");
            proc.stdin_from("are");
            usleep(1000);
            EXPECT_EQ(res.back(), "are");
            proc.stdin_from("you");
            usleep(1000);
            proc.stop();
            worker.stop_worker();
            EXPECT_EQ(res.back(), "you");
        }
    }

    TEST_F(TestProcess, test_process_simple)
    {
        Process proc{"ls", "-la"};

        EXPECT_FALSE(proc.wait_any());
        EXPECT_TRUE(proc.start());
        EXPECT_TRUE(proc.wait_any());
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_out)
    {
        std::string output;
        Process proc{"echo", "hello", "world"};

        proc.stdout_to([&] (const char *buffer, [[maybe_unused]] ssize_t size) { output = buffer; });
        EXPECT_EQ(output, "");
        EXPECT_TRUE(proc.start());
        EXPECT_EQ(output, "");
        EXPECT_TRUE(proc.wait_any());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world\n");

        output.clear();
        proc.clear();
        proc.stdout_to(output);
        EXPECT_TRUE(proc.start());
        EXPECT_TRUE(proc.wait_any());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world\n");

        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_in_cat)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        Process proc{"cat"};
        std::string output;

        proc.stdin_from("hello world");
        proc.stdout_to(output);
        EXPECT_TRUE(proc.start());
        proc.stdin_from("1");
        proc.stdin_from("2");
        proc.read_pipes();
        EXPECT_EQ(output, "hello world12");
        proc.stdin_from("3");
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world123");
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_in_wc)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy with valgrind";
        Process proc{"wc", "-c"};
        std::string result;

        proc.stdin_from("hello world");
        proc.stdout_to([&result] (const char *buf, size_t size)
        {
            (void)size;
            result = buf;
        });
        EXPECT_TRUE(proc.start());
        proc.stdin_close();
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(result, "11\n");
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_file_in)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy with valgrind";
        std::string test_dir = FS::combine(_base_test_dir, "in_file");
        std::filesystem::remove_all(test_dir);
        std::string test_file = FS::combine(test_dir, "hello.txt");
        std::filesystem::create_directories(FS::parent(test_file));

        SIHD_LOG(info, "Writing file for 'cat' input: " << test_file)
        EXPECT_TRUE(FS::write(test_file, "hello world"));

        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";

        Process proc{"cat"};
        std::string output;
        EXPECT_TRUE(proc.stdin_from_file(test_file));
        proc.stdout_to(output);
        EXPECT_TRUE(proc.start());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(output, "hello world");
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_file_out)
    {
        std::string test_dir = FS::combine(_base_test_dir, "to_file");
        std::filesystem::remove_all(test_dir);
        std::string test_file = FS::combine(test_dir, "output.txt");
        std::filesystem::create_directories(FS::parent(test_file));
        Process proc{"echo", "hello", "world"};

        EXPECT_TRUE(proc.stdout_to_file(test_file));
        EXPECT_EQ(FS::read_all(test_file).value(), "");
        EXPECT_TRUE(proc.start());
        EXPECT_TRUE(proc.wait_any());
        EXPECT_TRUE(proc.end());
        EXPECT_EQ(FS::read_all(test_file).value(), "hello world\n");
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }

    TEST_F(TestProcess, test_process_file_out_err)
    {
        std::string path = FS::combine(_base_test_dir, "file_out_err");
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
        std::string stdout_path = FS::combine(path, "stdout.txt");
        std::string stderr_path = FS::combine(path, "stderr.txt");

        Process proc([]() -> int {
            std::cout << "hello world";
            std::cerr << "nope";
            return 1;
        });

        std::string out;
        std::string err;
        EXPECT_TRUE(proc.stdout_to_file(stdout_path));
        EXPECT_TRUE(proc.stderr_to_file(stderr_path));

        SIHD_TRACE("Redirecting stdout to: " << stdout_path);
        SIHD_TRACE("Redirecting stderr to: " << stderr_path);

        EXPECT_TRUE(proc.start());
        EXPECT_TRUE(proc.wait_any());
        EXPECT_TRUE(proc.end());

        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 1);

        EXPECT_EQ(FS::read_all(stdout_path).value(), "hello world");
        EXPECT_EQ(FS::read_all(stderr_path).value(), "nope");
    }

    TEST_F(TestProcess, test_process_close)
    {
        Process ls{"ls", "-la"};
        ls.stdout_close().stderr_close();
        EXPECT_TRUE(ls.start());
        ls.wait_exit();
        EXPECT_TRUE(ls.end());
        EXPECT_TRUE(ls.has_exited());
        // bad file descriptor
        EXPECT_EQ(ls.return_code(), 2);

        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        Process cat{"cat"};
        cat.stdin_close().stderr_close();
        EXPECT_TRUE(cat.start());
        EXPECT_TRUE(cat.end());
        EXPECT_TRUE(cat.has_exited());
        // bad file descriptor
        EXPECT_EQ(cat.return_code(), 1);
    }

    TEST_F(TestProcess, test_process_chain)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        std::string output;
        Process echo{"echo", "hello world"};
        Process wc{"wc", "-c"};
        Process cat{"cat", "-e"};

        echo.stdout_to(wc);
        wc.stdout_to(cat);
        cat.stdout_to(output);

        EXPECT_TRUE(echo.start());
        EXPECT_TRUE(wc.start());
        EXPECT_TRUE(cat.start());

        EXPECT_TRUE(echo.wait_any());
        EXPECT_TRUE(echo.end());
        EXPECT_TRUE(wc.wait_any());
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
        EXPECT_TRUE(ls.start());
        ls.wait_exit();
        EXPECT_TRUE(ls.end());
        EXPECT_TRUE(ls.end());
        EXPECT_TRUE(ls.end());
        EXPECT_EQ(output, "ls: cannot access '/bli/blah/blouh': No such file or directory\n");
        EXPECT_TRUE(ls.has_exited());
        EXPECT_EQ(ls.return_code(), 2);
    }

    TEST_F(TestProcess, test_process_signal_kill)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        Process cat{"cat"};

        EXPECT_TRUE(cat.start());
        usleep(3000);
        EXPECT_TRUE(cat.kill(SIGTERM));
        cat.wait_exit();
        EXPECT_FALSE(cat.has_exited());
        EXPECT_TRUE(cat.has_exited_by_signal());
        SIHD_TRACE("Signal exit number: " << cat.signal_exit_number());
        EXPECT_EQ(cat.signal_exit_number(), SIGTERM);
    }

    TEST_F(TestProcess, test_process_signal_stop)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP() << "Is an interactive test";
        Process cat{"cat"};

        EXPECT_TRUE(cat.start());
        usleep(3000);
        EXPECT_TRUE(cat.kill(SIGSTOP));
        cat.wait_stop();
        EXPECT_TRUE(cat.has_stopped_by_signal());
        SIHD_TRACE("Signal stop number: " << cat.signal_stop_number());
        EXPECT_EQ(cat.signal_stop_number(), SIGSTOP);

        EXPECT_TRUE(cat.kill(SIGCONT));
        cat.wait_continue();

        EXPECT_TRUE(cat.kill(SIGTERM));
        cat.wait_exit();
        EXPECT_TRUE(cat.has_exited_by_signal());
        EXPECT_EQ(cat.signal_exit_number(), SIGTERM);
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

        EXPECT_TRUE(proc.start());
        EXPECT_TRUE(proc.wait_any());
        EXPECT_TRUE(proc.end());

        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 1);

        EXPECT_EQ(out, "hello world");
        EXPECT_EQ(err, "nope");
    }

    TEST_F(TestProcess, test_process_bad_cmd)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy under valgrind";
        Process proc{"ellesse", "-la"};

        EXPECT_FALSE(proc.start());
        EXPECT_FALSE(proc.wait_any());
    }

}
