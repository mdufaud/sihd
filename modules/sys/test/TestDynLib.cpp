#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

constexpr const char *test_libc = build::is_windows ? "msvcrt.dll" : build::is_android ? "libc.so" : "libc.so.6";

} // namespace

class TestDynLib: public ::testing::Test
{
    protected:
        TestDynLib() { LoggerManager::stream(); }

        virtual ~TestDynLib() { LoggerManager::clear_loggers(); }
};

TEST_F(TestDynLib, test_dynlib_open_close)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    EXPECT_FALSE(lib.is_open());

    EXPECT_TRUE(lib.open(test_libc).has_value());
    EXPECT_TRUE(lib.is_open());
    EXPECT_TRUE(lib.close());
    EXPECT_FALSE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_constructor_throws)
{
    EXPECT_THROW(DynLib("libnonexistent_12345.so"), std::runtime_error);
}

TEST_F(TestDynLib, test_dynlib_constructor_open)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib(test_libc);
    EXPECT_TRUE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_load_symbol)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    ASSERT_TRUE(lib.open(test_libc).has_value());

    auto sym = lib.load_symbol("printf");
    ASSERT_TRUE(sym.has_value());
    EXPECT_NE(*sym, nullptr);

    auto missing = lib.load_symbol("nonexistent_symbol_12345");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, not_found);
    EXPECT_FALSE(missing.error().message.empty());
}

TEST_F(TestDynLib, test_dynlib_load_function)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    ASSERT_TRUE(lib.open(test_libc).has_value());

    auto string_length = lib.load_function<size_t(const char *)>("strlen");
    ASSERT_TRUE(string_length.has_value());
    EXPECT_EQ((*string_length)("sihd"), 4u);

    auto missing = lib.load_function<void()>("nonexistent_symbol_12345");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, not_found);
}

TEST_F(TestDynLib, test_dynlib_open_nonexistent)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    auto opened = lib.open("libnonexistent_12345.so");
    EXPECT_FALSE(opened.has_value());
    // dlerror text classification falls back to io_error under a localized libc
    EXPECT_NE(opened.error().code, none);
    EXPECT_FALSE(opened.error().message.empty());
    EXPECT_FALSE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_open_system)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    EXPECT_TRUE(lib.open_system(test_libc).has_value());
    EXPECT_TRUE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_open_system_refuses_path)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    auto opened = lib.open_system(std::string("./") + test_libc);
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, invalid_argument);
    EXPECT_FALSE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_open_system_skips_current_directory)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    TmpDir tmp;
    ASSERT_TRUE(tmp);
    const std::string probe = build::is_windows ? "sihd_cwd_probe.dll" : "libsihd_cwd_probe.so";
    ASSERT_TRUE(fs::write(fs::combine(tmp.path(), probe), "not a library"));
    const std::string previous_directory = fs::cwd();
    ASSERT_TRUE(fs::chdir(tmp.path()));

    DynLib lib;
    auto opened = lib.open_system(probe);
    ASSERT_TRUE(fs::chdir(previous_directory));

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, not_found);
}

} // namespace test
