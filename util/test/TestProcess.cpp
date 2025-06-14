#include <filesystem>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/proc.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestProcess: public ::testing::Test
{
    protected:
        TestProcess() { sihd::util::LoggerManager::stream(); }

        virtual ~TestProcess() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        TmpDir _tmp_dir;
};

/*
TEST_F(TestProcess, test_process_interactive)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    std::string output;
    Process proc{"cat"};

    proc.stdout_to([] (std::string_view buffer)
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
    EXPECT_TRUE(proc.wait_process_terminate(time::sec(10)));
    proc.stop();
    worker.stop_worker();
}
*/

TEST_F(TestProcess, test_process_service)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    std::vector<std::string> res;
    std::string output;
    Process proc {"cat"};
    Waitable waitable;

    proc.stdin_from("hello")
        .stdout_to([&res, &waitable](std::string_view buffer) {
            auto l = waitable.guard();
            res.emplace_back(buffer);
            SIHD_LOG(debug, "{}", buffer);
        })
        .stderr_close();

    Worker worker([&proc]() { return proc.start(); });

    ASSERT_TRUE(worker.start_sync_worker("proc"));

    // waiting cat process to boot
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ASSERT_FALSE(res.empty());
    EXPECT_EQ(res.back(), "hello");

    auto cat_write_plus_polling_time = std::chrono::milliseconds(15);

    proc.stdin_from("world");
    waitable.wait_for(cat_write_plus_polling_time);
    EXPECT_EQ(res.back(), "world");

    proc.stdin_from("how");
    waitable.wait_for(cat_write_plus_polling_time);
    EXPECT_EQ(res.back(), "how");

    proc.stdin_from("are");
    waitable.wait_for(cat_write_plus_polling_time);
    EXPECT_EQ(res.back(), "are");

    proc.stdin_from("you");
    waitable.wait_for(cat_write_plus_polling_time);

    EXPECT_TRUE(proc.stop());
    EXPECT_TRUE(worker.stop_worker());

    EXPECT_EQ(res.back(), "you");
}

TEST_F(TestProcess, test_process_simple)
{
    Process proc {"ls", "-la"};

    EXPECT_FALSE(proc.wait_any());
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_out)
{
    const std::string progname = "echo";
    std::string output;
    Process proc {progname, "hello", "world"};

    proc.stdout_to([&output](std::string_view buffer) { output = buffer; });
    EXPECT_EQ(output, "");
    EXPECT_TRUE(proc.execute());
    EXPECT_EQ(output, "");
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, "hello world\n");

    output.clear();
    proc.clear();
    proc.stdout_to(output);
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, "hello world\n");

    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_in_cat)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    Process proc {"cat"};
    std::string output;

    proc.stdin_from("hello world");
    proc.stdout_to(output);
    EXPECT_TRUE(proc.execute());

    // cat starting time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    proc.stdin_from("1");
    proc.stdin_from("2");

    constexpr int ms_timeout = 20;

    // reading stdout
    proc.read_pipes(ms_timeout);
    // maybe there is '2' to read
    proc.read_pipes(ms_timeout);

    EXPECT_EQ(output, "hello world12");

    proc.stdin_from("3");
    proc.read_pipes(ms_timeout);
    EXPECT_EQ(output, "hello world123");

    EXPECT_TRUE(proc.terminate());
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_in_wc)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Process proc {"wc", "-c"};
    std::string result;

    proc.stdin_from("hello world");
    proc.stdout_to([&result](std::string_view buffer) { result = buffer; });

    EXPECT_TRUE(proc.execute());

    // wc boot time
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // close stdin to get wc output
    proc.stdin_close();

    // wait for wc to process the pipe closing and quit
    proc.wait_any();

    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(result, "11\n");
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_file_in)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    std::string test_file = fs::combine(_tmp_dir.path(), "file_in_hello.txt");

    SIHD_LOG(info, "Writing file for 'cat' input: {}", test_file)
    EXPECT_TRUE(fs::write(test_file, "hello world"));

    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";

    Process proc {"cat"};
    std::string output;
    EXPECT_TRUE(proc.stdin_from_file(test_file));
    proc.stdout_to(output);
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, "hello world");
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_file_out)
{
    std::string test_file = fs::combine(_tmp_dir.path(), "file_out_output.txt");

    Process proc {"echo", "hello", "world"};

    EXPECT_TRUE(proc.stdout_to_file(test_file));
    EXPECT_EQ(fs::read_all(test_file).value(), "");
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(fs::read_all(test_file).value(), "hello world\n");
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_file_out_err)
{
    std::string stdout_path = fs::combine(_tmp_dir.path(), "file_out_err_stdout.txt");
    std::string stderr_path = fs::combine(_tmp_dir.path(), "file_out_err_stderr.txt");

    Process proc([]() -> int {
        std::cout << "hello world";
        std::cerr << "nope";
        return 1;
    });

    std::string out;
    std::string err;
    EXPECT_TRUE(proc.stdout_to_file(stdout_path));
    EXPECT_TRUE(proc.stderr_to_file(stderr_path));

    SIHD_TRACE("Redirecting stdout to: {}", stdout_path);
    SIHD_TRACE("Redirecting stderr to: {}", stderr_path);

    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());

    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 1);

    EXPECT_EQ(fs::read_all(stdout_path).value(), "hello world");
    EXPECT_EQ(fs::read_all(stderr_path).value(), "nope");
}

TEST_F(TestProcess, test_process_close)
{
    Process ls {"ls", "-la"};
    ls.stdout_close().stderr_close();
    EXPECT_TRUE(ls.execute());
    ls.wait_exit();
    EXPECT_TRUE(ls.terminate());
    EXPECT_TRUE(ls.has_exited());
    // bad file descriptor
    EXPECT_EQ(ls.return_code(), 2);

    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    Process cat {"cat"};
    cat.stdin_close().stderr_close();
    EXPECT_TRUE(cat.execute());

    // wait cat boot
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(cat.terminate());
    EXPECT_TRUE(cat.has_exited());
    // bad file descriptor
    EXPECT_EQ(cat.return_code(), 1);
}

TEST_F(TestProcess, test_process_chain)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    std::string output;
    Process echo {"echo", "hello world"};
    Process wc {"wc", "-c"};
    Process cat {"cat", "-e"};

    echo.stdout_to(wc);
    wc.stdout_to(cat);
    cat.stdout_to(output);

    EXPECT_TRUE(echo.execute());
    EXPECT_TRUE(wc.execute());
    EXPECT_TRUE(cat.execute());

    EXPECT_TRUE(echo.wait_any());
    EXPECT_TRUE(echo.terminate());

    // wait for wc to process the pipe closing
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_TRUE(wc.wait_any());
    EXPECT_TRUE(wc.terminate());
    EXPECT_TRUE(cat.terminate());

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
    Process ls {"ls", "/bli/blah/blouh"};

    ls.stderr_to(output);
    EXPECT_TRUE(ls.execute());
    ls.wait_exit();
    EXPECT_TRUE(ls.terminate());
    EXPECT_TRUE(ls.terminate());
    EXPECT_TRUE(ls.terminate());
    EXPECT_EQ(output, "ls: cannot access '/bli/blah/blouh': No such file or directory\n");
    EXPECT_TRUE(ls.has_exited());
    EXPECT_EQ(ls.return_code(), 2);
}

TEST_F(TestProcess, test_process_signal_kill)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    Process cat {"cat"};

    EXPECT_TRUE(cat.execute());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(cat.kill(SIGTERM));
    cat.wait_exit();
    EXPECT_FALSE(cat.has_exited());
    EXPECT_TRUE(cat.has_exited_by_signal());
    SIHD_TRACE("Signal exit number: {}", cat.signal_exit_number());
    EXPECT_EQ(cat.signal_exit_number(), SIGTERM);
}

TEST_F(TestProcess, test_process_signal_stop)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    Process cat {"cat"};

    EXPECT_TRUE(cat.execute());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(cat.kill(SIGSTOP));
    cat.wait_stop();
    EXPECT_TRUE(cat.has_stopped_by_signal());
    SIHD_TRACE("Signal stop number: {}", cat.signal_stop_number());
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

        char *env = getenv("TOTO");
        if (env != nullptr)
            std::cout << " " << env;

        env = getenv("TO_REMOVE");
        if (env != nullptr)
            std::cerr << "= FAILED TO REMOVE ENV VAR = ";

        std::cerr << "nope";
        return 1;
    });

    proc.env_set("TOTO", "tata");
    EXPECT_EQ(proc.env_get("TOTO").value_or(""), "tata");
    proc.env_load({"TOTO=titi"});
    EXPECT_EQ(proc.env_get("TOTO").value_or(""), "titi");

    proc.env_set("TO_REMOVE", "-");
    proc.env_rm("TO_REMOVE");

    std::string out;
    std::string err;
    proc.stdout_to(out);
    proc.stderr_to(err);

    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());

    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ(proc.return_code(), 1);

    EXPECT_EQ(out, "hello world titi");
    EXPECT_EQ(err, "nope");
}

TEST_F(TestProcess, test_process_bad_cmd)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy under valgrind";
    Process proc {"ellesse", "-la"};
    EXPECT_FALSE(proc.execute());
    EXPECT_FALSE(proc.wait_any());
    EXPECT_EQ(proc.return_code(), 255);

    // Try with fork

    EXPECT_TRUE(proc.reset());
    proc.set_force_fork(true);

    // with fork you cannot know at the fork time if the binary is found or not
    // fork succeeded
    EXPECT_TRUE(proc.execute());
    // wait for child process fork to end
    EXPECT_TRUE(proc.wait_any());
    // the exit status
    EXPECT_EQ(proc.return_code(), 255);
}

TEST_F(TestProcess, test_process_exec_simple)
{
    auto exit_code = proc::execute({"echo", "hello", "world"});
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 0);
}

TEST_F(TestProcess, test_process_exec_stdout)
{
    std::string printed;
    proc::Options options({.stdout_callback = [&printed](std::string_view stdout_str) {
        printed += stdout_str;
    }});
    auto exit_code = proc::execute({"echo", "hello", "world"}, options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 0);
    EXPECT_EQ(printed, "hello world\n");
}

TEST_F(TestProcess, test_process_exec_stderr)
{
    std::string printed;
    proc::Options options({.stderr_callback = [&printed](std::string_view stderr_str) {
        printed += stderr_str;
    }});
    const std::string prog_name = "ls";
    auto exit_code = proc::execute({prog_name, "/there/is/nothing/here"}, options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 2);
    EXPECT_EQ(printed, "ls: cannot access '/there/is/nothing/here': No such file or directory\n");
}

TEST_F(TestProcess, test_process_exec_stdin)
{
    std::string stdin_input = "hello world";
    std::string printed;
    proc::Options options({.timeout = std::chrono::milliseconds(100),
                           .to_stdin = stdin_input,
                           .stdout_callback = [&printed](std::string_view stdout_str) {
                               printed += stdout_str;
                           }});
    auto exit_code = proc::execute({"cat"}, options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 0);
    EXPECT_EQ(printed, "hello world");
}

TEST_F(TestProcess, test_process_exec_timeout)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    proc::Options options({.timeout = std::chrono::milliseconds(80)});
    auto exit_code = proc::execute({"cat"}, options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(70)), std::future_status::ready);
    // kill by signal SIGTERM (9)
    ASSERT_EQ(exit_code.get(), 255);
}

TEST_F(TestProcess, test_process_exec_not_found)
{
    auto exit_code = proc::execute({"not-a-binary-that-exists"});
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    if (os::is_run_by_valgrind())
    {
        // Value 127 is returned by /bin/sh when the given command is not found within your PATH
        ASSERT_EQ(exit_code.get(), 127);
    }
    else
    {
        ASSERT_EQ(exit_code.get(), 255);
    }
}

} // namespace test
