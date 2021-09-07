#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Path.hpp>
#include <experimental/filesystem>
#include <iostream>
#include <fstream>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    using namespace std::experimental;
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
        filesystem::path test_path(getenv("TEST_PATH"));
        test_path.append("test_path");
        filesystem::remove_all(test_path);
        filesystem::path test_dir = test_path / "1" / "2";
        filesystem::create_directories(test_dir);
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

        TRACE(Path::find("test/TestPath.cpp"));
        filesystem::remove_all(test_path);
    }
}
