#include <fstream>
#include <filesystem>

#include <gtest/gtest.h>

#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Path.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestPath:   public ::testing::Test
    {
        protected:
            TestPath()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestPath()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
                Path::clear_all();
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestPath, test_path_get)
    {
        TmpDir tmp_dir;

        std::filesystem::path test_path(tmp_dir.path());
        test_path.append("test_path");
        std::filesystem::path test_dir = test_path / "1" / "2";
        std::filesystem::create_directories(test_dir);
        auto test_file = test_dir / "file.txt";
        std::ofstream file(test_file.generic_string());
        file << "hello" << std::endl;
        file.close();

        Path::set("", "");
        Path::set("false-dir", "/does/not/exists");
        Path::set("sihd-test", test_dir.generic_string());
        EXPECT_EQ(Path::get("sihd-test", "file.txt"), test_dir / "file.txt");
        EXPECT_EQ(Path::get("sihd-test://file.txt"), test_dir / "file.txt");
        EXPECT_EQ(Path::find("file.txt"), test_dir / "file.txt");

        Path::clear("sihd-test");
        EXPECT_EQ(Path::get("sihd-test", "file.txt"), "");

        SIHD_TRACEF(Path::find("test/TestPath.cpp"));
    }
}
