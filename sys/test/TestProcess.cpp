#include <cstdlib>
#include <filesystem>

#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/term.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

// list current directory, returns 0
std::vector<std::string> cmd_list()
{
#if defined(__SIHD_WINDOWS__)
    return {"cmd", "/c", "dir"};
#else
    return {"ls", "-la"};
#endif
}

// print "hello world" on stdout, returns 0
std::vector<std::string> cmd_echo_hello_world()
{
#if defined(__SIHD_WINDOWS__)
    return {"cmd", "/c", "echo", "hello", "world"};
#else
    return {"echo", "hello", "world"};
#endif
}

// stdin -> stdout passthrough (windows sort only flushes at stdin EOF)
std::vector<std::string> cmd_stdin_passthrough()
{
#if defined(__SIHD_WINDOWS__)
    return {"sort"};
#else
    return {"cat"};
#endif
}

// access a path that does not exist -> non-empty stderr + non-zero return
std::vector<std::string> cmd_bad_path()
{
#if defined(__SIHD_WINDOWS__)
    return {"cmd", "/c", "type", "Z:\\bli\\blah\\blouh"};
#else
    return {"ls", "/bli/blah/blouh"};
#endif
}

// long running process (to be killed by timeout)
std::vector<std::string> cmd_sleep()
{
    return {helper_path(), "sleep", "10"};
}

#if defined(__SIHD_WINDOWS__)
constexpr const char *expected_hello_world = "hello world\r\n";
#else
constexpr const char *expected_hello_world = "hello world\n";
#endif

} // namespace

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
    Process proc(cmd_stdin_passthrough());
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

    // waiting passthrough process to boot
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

#if !defined(__SIHD_WINDOWS__)
    // cat echoes each write back immediately
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
#else
    // sort only flushes at stdin EOF: feed everything, close stdin, collect once
    proc.stdin_from("world");
    proc.stdin_from("how");
    proc.stdin_from("are");
    proc.stdin_from("you");
    proc.stdin_close();

    waitable.wait_for(std::chrono::milliseconds(500), [&res] { return !res.empty(); });

    EXPECT_TRUE(proc.stop());
    EXPECT_TRUE(worker.stop_worker());

    ASSERT_FALSE(res.empty());
    EXPECT_EQ(res.back(), "helloworldhowareyou");
#endif
}

TEST_F(TestProcess, test_process_simple)
{
    Process proc(cmd_list());

    EXPECT_FALSE(proc.wait_any());
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ((int)proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_out)
{
    std::string output;
    Process proc(cmd_echo_hello_world());

    proc.stdout_to([&output](std::string_view buffer) { output = buffer; });
    EXPECT_EQ(output, "");
    EXPECT_TRUE(proc.execute());
    EXPECT_EQ(output, "");
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, expected_hello_world);

    output.clear();
    proc.clear();
    proc.stdout_to(output);
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, expected_hello_world);

    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ((int)proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_in_cat)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    Process proc(cmd_stdin_passthrough());
    std::string output;

    proc.stdin_from("hello world");
    proc.stdout_to(output);
    EXPECT_TRUE(proc.execute());

    // passthrough starting time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    proc.stdin_from("1");
    proc.stdin_from("2");

    constexpr int ms_timeout = 20;

#if !defined(__SIHD_WINDOWS__)
    // cat echoes back each write immediately
    // reading stdout
    proc.read_pipes(ms_timeout);
    // maybe there is '2' to read
    proc.read_pipes(ms_timeout);

    EXPECT_EQ(output, "hello world12");

    proc.stdin_from("3");
    proc.read_pipes(ms_timeout);
    EXPECT_EQ(output, "hello world123");

    EXPECT_TRUE(proc.terminate());
#else
    // sort only flushes at stdin EOF
    proc.stdin_from("3");
    proc.stdin_close();
    proc.wait_any();
    proc.read_pipes(ms_timeout);
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, "hello world123");
#endif
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ((int)proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_in_wc)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

#if !defined(__SIHD_WINDOWS__)
    Process proc {"wc", "-c"};
    constexpr const char *expected_result = "11\n";
#else
    // no byte-count command on windows: passthrough still proves
    // that closing stdin flushes the child and ends it
    Process proc(cmd_stdin_passthrough());
    constexpr const char *expected_result = "hello world";
#endif
    std::string result;

    proc.stdin_from("hello world");
    proc.stdout_to([&result](std::string_view buffer) { result = buffer; });

    EXPECT_TRUE(proc.execute());

    // child boot time
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // close stdin to get the child's output
    proc.stdin_close();

    // wait for the child to process the pipe closing and quit
    proc.wait_any();

    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(result, expected_result);
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ((int)proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_file_in)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    std::string test_file = fs::combine(_tmp_dir.path(), "file_in_hello.txt");

    SIHD_LOG(info, "Writing file for 'cat' input: {}", test_file);
    EXPECT_TRUE(fs::write(test_file, "hello world"));

    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";

    Process proc(cmd_stdin_passthrough());
    std::string output;
    EXPECT_TRUE(proc.stdin_from_file(test_file));
    proc.stdout_to(output);
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    EXPECT_EQ(output, "hello world");
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ((int)proc.return_code(), 0);
}

TEST_F(TestProcess, test_process_file_out)
{
    std::string test_file = fs::combine(_tmp_dir.path(), "file_out_output.txt");

    Process proc(cmd_echo_hello_world());

    EXPECT_TRUE(proc.stdout_to_file(test_file));
    EXPECT_EQ(fs::read_all(test_file).value(), "");
    EXPECT_TRUE(proc.execute());
    EXPECT_TRUE(proc.wait_any());
    EXPECT_TRUE(proc.terminate());
    // fs::read_all opens in text mode, so CRLF is normalized to LF on every platform
    EXPECT_EQ(fs::read_all(test_file).value(), "hello world\n");
    EXPECT_TRUE(proc.has_exited());
    EXPECT_EQ((int)proc.return_code(), 0);
}

#if !defined(__SIHD_WINDOWS__)
// windows _do_child_process uses CreateProcess and cannot run an in-process
// function child (no fork) -> function-based Process stays unix-only
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
    EXPECT_EQ((int)proc.return_code(), 1);

    EXPECT_EQ(fs::read_all(stdout_path).value(), "hello world");
    EXPECT_EQ(fs::read_all(stderr_path).value(), "nope");
}
#endif

TEST_F(TestProcess, test_process_close)
{
    Process proc(cmd_list());
    proc.stdout_close().stderr_close();
    EXPECT_TRUE(proc.execute());
    proc.wait_exit();
    EXPECT_TRUE(proc.terminate());
    EXPECT_TRUE(proc.has_exited());
#if !defined(__SIHD_WINDOWS__)
    // bad file descriptor: ls fails writing to the closed stdout/stderr
    EXPECT_EQ((int)proc.return_code(), 2);
#endif

    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    Process cat(cmd_stdin_passthrough());
    cat.stdin_close().stderr_close();
    EXPECT_TRUE(cat.execute());

    // wait child boot
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(cat.terminate());
    EXPECT_TRUE(cat.has_exited());
#if !defined(__SIHD_WINDOWS__)
    // bad file descriptor
    EXPECT_EQ((int)cat.return_code(), 1);
#else
    // sort sees EOF on the closed stdin and exits cleanly
    EXPECT_EQ((int)cat.return_code(), 0);
#endif
}

TEST_F(TestProcess, test_process_chain)
{
    if (term::is_interactive() == false)
        GTEST_SKIP() << "Is an interactive test";
    std::string output;
#if !defined(__SIHD_WINDOWS__)
    Process echo {"echo", "hello world"};
    Process wc {"wc", "-c"};
    Process cat {"cat", "-e"};
    constexpr const char *expected_chain = "12$\n";
#else
    // no wc/cat on windows: chain two sort passthroughs behind echo
    Process echo {"cmd", "/c", "echo", "hello", "world"};
    Process wc {"sort"};
    Process cat {"sort"};
    constexpr const char *expected_chain = "hello world\r\n";
#endif

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

    EXPECT_EQ(output, expected_chain);

    EXPECT_TRUE(echo.has_exited());
    EXPECT_EQ((int)echo.return_code(), 0);

    EXPECT_TRUE(wc.has_exited());
    EXPECT_EQ((int)wc.return_code(), 0);

    EXPECT_TRUE(cat.has_exited());
    EXPECT_EQ((int)cat.return_code(), 0);
}

TEST_F(TestProcess, test_process_stderr)
{
    std::string output;
    Process proc(cmd_bad_path());

    proc.stderr_to(output);
    EXPECT_TRUE(proc.execute());
    proc.wait_exit();
    EXPECT_TRUE(proc.terminate());
    EXPECT_TRUE(proc.terminate());
    EXPECT_TRUE(proc.terminate());
    EXPECT_TRUE(proc.has_exited());
#if !defined(__SIHD_WINDOWS__)
    EXPECT_EQ(output, "ls: cannot access '/bli/blah/blouh': No such file or directory\n");
    EXPECT_EQ((int)proc.return_code(), 2);
#else
    // windows error text differs -> assert the invariant: stderr produced, non-zero return
    EXPECT_FALSE(output.empty());
    EXPECT_NE((int)proc.return_code(), 0);
#endif
}

#if !defined(__SIHD_WINDOWS__)
// signal-exit semantics (has_exited_by_signal / signal_exit_number) are unix-only:
// windows TerminateProcess makes the child exit with a code, never "by signal"
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

// SIGSTOP / SIGCONT have no windows equivalent
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

// windows cannot run an in-process function child (no fork)
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
    EXPECT_EQ((int)proc.return_code(), 1);

    EXPECT_EQ(out, "hello world titi");
    EXPECT_EQ(err, "nope");
}
#endif

TEST_F(TestProcess, test_process_bad_cmd)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy under valgrind";
    Process proc {"ellesse", "-la"};
    if (os::is_run_by_qemu())
    {
        // qemu breaks posix_spawn's synchronous exec-failure report: the spawn "succeeds",
        // the child execve fails and _exit(127), same as the fork path below
        EXPECT_TRUE(proc.execute());
        EXPECT_TRUE(proc.wait_any());
        EXPECT_EQ((int)proc.return_code(), 127);
    }
    else
    {
        EXPECT_FALSE(proc.execute());
        EXPECT_FALSE(proc.wait_any());
        EXPECT_EQ((int)proc.return_code(), 255);
    }

#if !defined(__SIHD_WINDOWS__)
    // Try with fork (windows _do_execute ignores _force_fork: no fork available)

    EXPECT_TRUE(proc.reset());
    proc.set_force_fork(true);

    // with fork you cannot know at the fork time if the binary is found or not
    // fork succeeded
    EXPECT_TRUE(proc.execute());
    // wait for child process fork to end
    EXPECT_TRUE(proc.wait_any());
    // the exit status
    EXPECT_EQ((int)proc.return_code(), 255);
#endif
}

TEST_F(TestProcess, test_process_exec_simple)
{
    auto exit_code = proc::execute(cmd_echo_hello_world());
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 0);
}

TEST_F(TestProcess, test_process_exec_stdout)
{
    std::string printed;
    proc::Options options({.stdout_callback = [&printed](std::string_view stdout_str) { printed += stdout_str; }});
    auto exit_code = proc::execute(cmd_echo_hello_world(), options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 0);
    EXPECT_EQ(printed, expected_hello_world);
}

TEST_F(TestProcess, test_process_exec_stderr)
{
    std::string printed;
    proc::Options options({.stderr_callback = [&printed](std::string_view stderr_str) { printed += stderr_str; }});
    auto exit_code = proc::execute(cmd_bad_path(), options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
#if !defined(__SIHD_WINDOWS__)
    ASSERT_EQ(exit_code.get(), 2);
    EXPECT_EQ(printed, "ls: cannot access '/bli/blah/blouh': No such file or directory\n");
#else
    EXPECT_NE(exit_code.get(), 0);
    EXPECT_FALSE(printed.empty());
#endif
}

TEST_F(TestProcess, test_process_exec_stdin)
{
    std::string stdin_input = "hello world";
    std::string printed;
    proc::Options options({.timeout = std::chrono::milliseconds(100),
                           .to_stdin = stdin_input,
                           .stdout_callback = [&printed](std::string_view stdout_str) { printed += stdout_str; }});
    auto exit_code = proc::execute(cmd_stdin_passthrough(), options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    ASSERT_EQ(exit_code.get(), 0);
    EXPECT_EQ(printed, "hello world");
}

TEST_F(TestProcess, test_process_exec_timeout)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    proc::Options options({.timeout = std::chrono::milliseconds(80)});
    auto exit_code = proc::execute(cmd_sleep(), options);
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    // wide margin: under wine/qemu the kill + reap after the 80ms timeout lands late
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(1000)), std::future_status::ready);
    // killed by signal / terminated
    ASSERT_EQ(exit_code.get(), 255);
}

TEST_F(TestProcess, test_process_exec_not_found)
{
    auto exit_code = proc::execute({"not-a-binary-that-exists"});
    ASSERT_EQ(exit_code.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    if (os::is_run_by_valgrind() || os::is_run_by_qemu())
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
