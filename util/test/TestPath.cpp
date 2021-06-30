#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Path.hpp>

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
        filesystem::path test_dir = test_path / "1/2";
        filesystem::remove_all(test_dir);
        filesystem::create_directories(test_dir);
        std::ofstream file(test_dir / "file.txt");
        file << "hello" << std::endl;
        file.close();

        Path::set("", "");
        Path::set("false-dir", "/does/not/exists");
        Path::set("sihd-test", test_dir);
        EXPECT_EQ(Path::get("sihd-test", "file.txt"), test_dir / "file.txt");
        EXPECT_EQ(Path::get("sihd-test://file.txt"), test_dir / "file.txt");
        EXPECT_EQ(Path::find("file.txt"), test_dir / "file.txt");

        Path::clear("sihd-test");
        EXPECT_EQ(Path::get("sihd-test", "file.txt"), "");

        Path::set("current-module", getenv("TEST_MODULE_PATH"));
        TRACE(Path::find("TestPath.cpp"));
    }
}
