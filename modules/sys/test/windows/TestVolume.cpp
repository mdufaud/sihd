#include <windows.h>

#include <array>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/windows/volume.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::windows_;

class TestVolume: public ::testing::Test
{
    protected:
        TestVolume() { LoggerManager::stream(); }

        virtual ~TestVolume() { LoggerManager::clear_loggers(); }
};

TEST_F(TestVolume, test_query)
{
    std::array<char, MAX_PATH + 1> windows_dir {};
    ASSERT_NE(GetWindowsDirectoryA(windows_dir.data(), static_cast<UINT>(windows_dir.size())), 0u);
    const std::string expected_root = std::string(windows_dir.data(), 3);

    auto info = volume::query(windows_dir.data());

    ASSERT_TRUE(info.has_value()) << info.error().message;
    EXPECT_EQ(info->root, expected_root);
    EXPECT_EQ(info->media_type, volume::MediaType::fixed);
    EXPECT_FALSE(info->file_system.empty());
}

TEST_F(TestVolume, test_query_missing_drive)
{
    const DWORD drives = GetLogicalDrives();
    char missing = '\0';
    for (char letter = 'Z'; letter >= 'D' && missing == '\0'; --letter)
    {
        if ((drives & (1u << (letter - 'A'))) == 0)
            missing = letter;
    }
    if (missing == '\0')
        GTEST_SKIP() << "every drive letter is mapped";

    auto info = volume::query(fmt::format("{}:\\nowhere", missing));

    ASSERT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::not_found) << info.error().message;
}

} // namespace test
