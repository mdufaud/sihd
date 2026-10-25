#include <windows.h>

#include <array>

#include <gtest/gtest.h>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestDynLibSearch: public ::testing::Test
{
    protected:
        TestDynLibSearch() { LoggerManager::stream(); }

        virtual ~TestDynLibSearch()
        {
            SetDllDirectoryA(nullptr);
            LoggerManager::clear_loggers();
        }
};

TEST_F(TestDynLibSearch, test_open_system_ignores_dll_directory)
{
    TmpDir tmp;
    std::array<char, MAX_PATH + 1> system_dir {};
    ASSERT_NE(GetSystemDirectoryA(system_dir.data(), static_cast<UINT>(system_dir.size())), 0u);
    ASSERT_TRUE(fs::copy_file(fs::combine(system_dir.data(), "msvcrt.dll"), fs::combine(tmp.path(), "sihd_probe.dll")));
    ASSERT_TRUE(SetDllDirectoryA(tmp.path().c_str()));

    DynLib lib;
    ASSERT_TRUE(lib.open("sihd_probe").has_value());
    ASSERT_TRUE(lib.close());

    auto opened = lib.open_system("sihd_probe");
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, not_found);
    EXPECT_FALSE(lib.is_open());
}

TEST_F(TestDynLibSearch, test_open_system_refuses_drive_relative)
{
    DynLib lib;

    auto opened = lib.open_system("C:msvcrt.dll");
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, invalid_argument);

    auto backslash = lib.open_system(".\\msvcrt.dll");
    ASSERT_FALSE(backslash.has_value());
    EXPECT_EQ(backslash.error().code, invalid_argument);
}

} // namespace test
