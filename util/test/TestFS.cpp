#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Num.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestFS:   public ::testing::Test
    {
        protected:
            TestFS()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestFS()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
                FS::sep = this->original_slash;
            }

            bool    log_make_dirs(std::string path)
            {
                SIHD_LOG(debug, "Making directory: " << path);
                return FS::make_directories(path);
            }

            char original_slash = FS::sep;
    };

    TEST_F(TestFS, test_fs_path)
    {
        FS::sep = '/';
        EXPECT_EQ(FS::combine("path", "test"), "path/test");
        EXPECT_EQ(FS::combine("path/to", "test"), "path/to/test");
        EXPECT_EQ(FS::combine("/path/to", "test"), "/path/to/test");
        EXPECT_EQ(FS::combine("/path/to/", "test"), "/path/to/test");
        EXPECT_EQ(FS::combine("/path/to//", "test"), "/path/to//test");
        EXPECT_EQ(FS::combine("/path/to/", "/test"), "/path/to//test");
        EXPECT_EQ(FS::combine("/path/to", "/test"), "/path/to//test");
        EXPECT_EQ(FS::combine("/", "test"), "/test");
        EXPECT_EQ(FS::combine("", "test"), "test");
        EXPECT_EQ(FS::combine("test", ""), "test/");

        EXPECT_EQ(FS::combine({"path", "to", "test"}), "path/to/test");

        EXPECT_EQ(FS::extension("/path/to/test.txt"), "txt");
        EXPECT_EQ(FS::extension("/path/to/test"), "");
        EXPECT_EQ(FS::extension(""), "");
        EXPECT_EQ(FS::extension("archive.tar.gz"), "tar.gz");
        EXPECT_EQ(FS::extension("/path/to/archive.tar.gz"), "tar.gz");

        EXPECT_EQ(FS::filename("/path/to/test.txt"), "test.txt");
        EXPECT_EQ(FS::filename("/path/to/////test.txt"), "test.txt");
        EXPECT_EQ(FS::filename("test.txt"), "test.txt");
        EXPECT_EQ(FS::filename(""), "");

        EXPECT_EQ(FS::parent("/path/to/test.txt"), "/path/to");
        EXPECT_EQ(FS::parent("/path/to////test.txt"), "/path/to");
        EXPECT_EQ(FS::parent("/path/to"), "/path");
        EXPECT_EQ(FS::parent("/path/to/"), "/path");
        EXPECT_EQ(FS::parent("/path/to////"), "/path");
        EXPECT_EQ(FS::parent("/path"), "");
        EXPECT_EQ(FS::parent("////path"), "");
        EXPECT_EQ(FS::parent("////path//"), "");
        EXPECT_EQ(FS::parent("/"), "");
        EXPECT_EQ(FS::parent("//////"), "");
        EXPECT_EQ(FS::parent("filename.txt"), "");
        EXPECT_EQ(FS::parent(""), "");

        EXPECT_EQ(FS::trim_path("/path/to/test", "/path/to"), "test");
        EXPECT_EQ(FS::trim_path("/path/to/test", "/path/to"), "test");
        std::string test("/path/to/test");
        EXPECT_EQ(FS::trim_path(test, "/path/"), "to/test");
        FS::trim_in_path(test, "/path");
        EXPECT_EQ(test, "to/test");
        std::vector<std::string> dirs = {
            "some/default/path/to/test",
            "some/default/path/another/test",
            "some/default/path/last",
        };
        FS::trim_in_path(dirs, "some/default/path");
        EXPECT_EQ(dirs[0], "to/test");
        EXPECT_EQ(dirs[1], "another/test");
        EXPECT_EQ(dirs[2], "last");
        EXPECT_EQ(FS::trim_path("path/to/test", "no_match/to/test"), "path/to/test");
        EXPECT_EQ(FS::trim_path("path/to/test", "to/"), "test");
        EXPECT_EQ(FS::trim_path("", "to/"), "");
        EXPECT_EQ(FS::trim_path("test", ""), "test");
        EXPECT_EQ(FS::trim_path("test", "/"), "test");
        EXPECT_EQ(FS::trim_path("test", "test"), "");

        EXPECT_EQ(FS::normalize("/path/to/the/../test"), "/path/to/test");
        EXPECT_EQ(FS::normalize("path/to/the/../test"), "path/to/test");
        EXPECT_EQ(FS::normalize("path///to//..///..///test"), "test");
        EXPECT_EQ(FS::normalize("path/to/..///../..///test"), "../test");
        EXPECT_EQ(FS::normalize("path/.."), "");
        EXPECT_EQ(FS::normalize("/path/.."), "/");
        EXPECT_EQ(FS::normalize(".."), "..");
        EXPECT_EQ(FS::normalize(""), "");

        FS::sep = '\\';
        EXPECT_EQ(FS::filename("\\path\\to\\test.txt"), "test.txt");
        EXPECT_EQ(FS::parent("\\path\\to\\test.txt"), "\\path\\to");
    }

    TEST_F(TestFS, test_fs_creation)
    {
        auto tmp_path = std::filesystem::temp_directory_path() / Str::to_hex(Num::rand());
        ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
        auto tmp_path_str = tmp_path.string();

        EXPECT_TRUE(FS::exists(tmp_path_str));
        EXPECT_TRUE(FS::is_dir(tmp_path_str));
        EXPECT_FALSE(FS::is_file(tmp_path_str));
        std::string sandbox_path = FS::combine({tmp_path_str, "creation"});
        EXPECT_TRUE(this->log_make_dirs(sandbox_path));
        EXPECT_TRUE(FS::exists(sandbox_path));
        EXPECT_TRUE(FS::is_dir(sandbox_path));
        EXPECT_FALSE(FS::is_file(sandbox_path));
        EXPECT_TRUE(this->log_make_dirs(sandbox_path));

        std::string path_to_test = FS::combine({sandbox_path, "path", "to", "test"});
        EXPECT_TRUE(this->log_make_dirs(path_to_test));
        EXPECT_TRUE(FS::exists(path_to_test));
        EXPECT_TRUE(FS::is_dir(path_to_test));
        std::ofstream file1(FS::combine({sandbox_path, "path", "file1.txt"}));
        std::ofstream file2(FS::combine({sandbox_path, "path", "to", "file2.txt"}));
        std::ofstream file3(FS::combine({sandbox_path, "path", "to", "test", "file3.txt"}));
        EXPECT_TRUE(file1.is_open());
        EXPECT_TRUE(file2.is_open());
        EXPECT_TRUE(file3.is_open());

        std::vector<std::string> rec_children = FS::recursive_children(sandbox_path);
        for (const auto & child: rec_children)
        {
            SIHD_LOG(debug, "recursive_children: " << child);
        }
        EXPECT_EQ(rec_children.size(), 6u);

        std::vector<std::string> children = FS::children(FS::combine(sandbox_path, "path"));
        for (const auto & child: children)
        {
            SIHD_LOG(debug, "children: " << child);
        }
        EXPECT_EQ(children.size(), 2u);

        std::string dirname = FS::combine(sandbox_path, "random_dir");
        EXPECT_FALSE(FS::is_dir(dirname));
        EXPECT_TRUE(log_make_dirs(dirname));
        EXPECT_TRUE(FS::is_dir(dirname));
        EXPECT_TRUE(FS::remove_directory(dirname));
        EXPECT_FALSE(FS::is_dir(dirname));

        EXPECT_TRUE(FS::remove_directories(sandbox_path));
        EXPECT_FALSE(FS::is_dir(path_to_test));
        EXPECT_TRUE(FS::is_dir(sandbox_path));
    }

    TEST_F(TestFS, test_fs_fast_io)
    {
        auto tmp_path = std::filesystem::temp_directory_path() / Str::to_hex(Num::rand());
        ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
        std::string path = FS::combine({tmp_path, "io", "test.txt"});

        std::string file_content = "hello world\n";
        EXPECT_TRUE(Str::ends_with(path, "/io/test.txt"));
        EXPECT_TRUE(this->log_make_dirs(FS::parent(path)));
        SIHD_LOG(info, "Writing file to: " << path);
        EXPECT_TRUE(FS::write(path, file_content));
        EXPECT_EQ(FS::read_all(path), file_content);
        EXPECT_EQ(FS::read(path, 5), "hello");
        char data[10];
        EXPECT_EQ(FS::read_binary(path, data, 5), 5);
        data[5] = 0;
        EXPECT_STREQ(data, "hello");

        EXPECT_EQ(FS::read_all("/there/is/no/path.txt"), std::nullopt);
        EXPECT_FALSE(FS::write("/there/is/no/path.txt", "none"));
        EXPECT_EQ(FS::read_all("/there/is/no/path.txt"), std::nullopt);
    }

}
