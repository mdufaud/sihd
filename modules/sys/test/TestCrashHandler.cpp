#include <array>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/CrashHandler.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <signal.h>
#endif

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestCrashHandler: public ::testing::Test
{
    protected:
        TestCrashHandler() { LoggerManager::stream(); }

        virtual ~TestCrashHandler() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestCrashHandler, test_previous_alt_stack_restored)
{
    stack_t original;
    ASSERT_EQ(sigaltstack(nullptr, &original), 0);
    std::array<char, 65536> buffer;
    stack_t previous {};
    previous.ss_sp = buffer.data();
    previous.ss_size = buffer.size();
    ASSERT_EQ(sigaltstack(&previous, nullptr), 0);
    Defer restore([&] { sigaltstack(&original, nullptr); });
    CrashHandler handler;
    ASSERT_TRUE(handler.install({}));
    handler.uninstall();
    stack_t restored;
    ASSERT_EQ(sigaltstack(nullptr, &restored), 0);
    EXPECT_EQ(restored.ss_sp, buffer.data());
    EXPECT_EQ(restored.ss_size, 65536u);
    EXPECT_EQ(restored.ss_flags, 0);
}
#endif

TEST_F(TestCrashHandler, test_crash_handler_raii)
{
    {
        CrashHandler handler(CrashHandler::Options {});
        EXPECT_TRUE(handler.is_installed());
    }
    CrashHandler handler;
    EXPECT_FALSE(handler.is_installed());
    EXPECT_TRUE(handler.install({}));
    EXPECT_TRUE(handler.is_installed());
    handler.uninstall();
    EXPECT_FALSE(handler.is_installed());
    // uninstalling twice is a no-op
    handler.uninstall();
}

TEST_F(TestCrashHandler, test_crash_handler_already_installed)
{
    CrashHandler handler;
    CrashHandler other;
    EXPECT_TRUE(handler.install({}));
    EXPECT_FALSE(other.is_installed());
    EXPECT_FALSE(other.install({}));
}

TEST_F(TestCrashHandler, test_crash_handler_dump)
{
    const std::string dump_path = fs::combine(fs::tmp_path(),
                                              "sihd_crash_" + str::generate_random_printable(8) + ".dmp");

    std::string output;
    proc::Options options;
    options.stderr_callback = [&](std::string_view view) {
        output.append(view);
    };
    const std::vector<std::string> args = {helper_path(), "crash", dump_path};
    auto exit_code_future = proc::execute(args, options);
    const int exit_code = exit_code_future.get();

    if constexpr (build::is_windows)
    {
        // access violation is a non-zero exit code
        EXPECT_NE(exit_code, 0);
        const auto size = fs::file_size(dump_path);
        ASSERT_TRUE(size.has_value());
        EXPECT_GT(*size, 0u);
    }
    else
    {
        // rethrow disabled: the handler exits with 128 + SIGSEGV
        EXPECT_EQ(exit_code, 128 + 11);
        // the dump file carries the text report
        const auto report = fs::read_all(dump_path);
        ASSERT_TRUE(report.has_value());
        EXPECT_NE(report->find("fatal signal 11"), std::string::npos);
        if constexpr (os::backtrace_supported)
        {
            EXPECT_NE(report->find("backtrace"), std::string::npos);
        }
    }
    EXPECT_NE(output.find("fatal "), std::string::npos);
    EXPECT_EQ(output.find("ThreadSanitizer"), std::string::npos);
    SIHD_LOG(debug, "crash output: {}", output);

    (void)fs::remove_file(dump_path);
}

} // namespace test
