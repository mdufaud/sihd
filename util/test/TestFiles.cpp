#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Array.hpp>
#include <filesystem>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestFiles:   public ::testing::Test
    {
        protected:
            TestFiles()
            {
                sihd::util::LoggerManager::basic();
                _test_path = getenv("TEST_PATH");
                auto path = std::filesystem::path(_test_path) / "util_files";
                _base_test_dir = path.generic_string();
            }

            virtual ~TestFiles()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
                std::filesystem::create_directory(_base_test_dir);
            }

            virtual void TearDown()
            {
                Files::sep = this->original_slash;
                std::filesystem::remove_all(_base_test_dir);
            }

            bool    log_make_dirs(std::string path)
            {
                LOG(debug, "Making directory: " << path);
                return Files::make_directories(path);
            }

            std::string _base_test_dir;
            std::string _test_path;

            char original_slash = Files::sep;
    };

    TEST_F(TestFiles, test_files_path)
    {
        Files::sep = '/';
        EXPECT_EQ(Files::combine("path", "test"), "path/test");
        EXPECT_EQ(Files::combine("path/to", "test"), "path/to/test");
        EXPECT_EQ(Files::combine("/path/to", "test"), "/path/to/test");
        EXPECT_EQ(Files::combine("/path/to/", "test"), "/path/to/test");
        EXPECT_EQ(Files::combine("/path/to//", "test"), "/path/to//test");
        EXPECT_EQ(Files::combine("/path/to/", "/test"), "/path/to//test");
        EXPECT_EQ(Files::combine("/path/to", "/test"), "/path/to//test");
        EXPECT_EQ(Files::combine("/", "test"), "/test");
        EXPECT_EQ(Files::combine("", "test"), "test");
        EXPECT_EQ(Files::combine("test", ""), "test/");

        EXPECT_EQ(Files::combine({"path", "to", "test"}), "path/to/test");

        EXPECT_EQ(Files::get_extension("/path/to/test.txt"), "txt");
        EXPECT_EQ(Files::get_extension("/path/to/test"), "");
        EXPECT_EQ(Files::get_extension(""), "");
        EXPECT_EQ(Files::get_extension("archive.tar.gz"), "tar.gz");
        EXPECT_EQ(Files::get_extension("/path/to/archive.tar.gz"), "tar.gz");

        EXPECT_EQ(Files::get_filename("/path/to/test.txt"), "test.txt");
        EXPECT_EQ(Files::get_filename("/path/to/////test.txt"), "test.txt");
        EXPECT_EQ(Files::get_filename("test.txt"), "test.txt");
        EXPECT_EQ(Files::get_filename(""), "");

        EXPECT_EQ(Files::get_parent("/path/to/test.txt"), "/path/to");
        EXPECT_EQ(Files::get_parent("/path/to////test.txt"), "/path/to");
        EXPECT_EQ(Files::get_parent("/path/to"), "/path");
        EXPECT_EQ(Files::get_parent("/path/to/"), "/path");
        EXPECT_EQ(Files::get_parent("/path/to////"), "/path");
        EXPECT_EQ(Files::get_parent("/path"), "");
        EXPECT_EQ(Files::get_parent("////path"), "");
        EXPECT_EQ(Files::get_parent("////path//"), "");
        EXPECT_EQ(Files::get_parent("/"), "");
        EXPECT_EQ(Files::get_parent("//////"), "");
        EXPECT_EQ(Files::get_parent("filename.txt"), "");
        EXPECT_EQ(Files::get_parent(""), "");

        EXPECT_EQ(Files::trim_path("/path/to/test", "/path/to"), "test");
        EXPECT_EQ(Files::trim_path("/path/to/test", "/path/to"), "test");
        std::string test("/path/to/test");
        EXPECT_EQ(Files::trim_path(test, "/path/"), "to/test");
        Files::trim_in_path(test, "/path");
        EXPECT_EQ(test, "to/test");
        std::vector<std::string> dirs = {
            "some/default/path/to/test",
            "some/default/path/another/test",
            "some/default/path/last",
        };
        Files::trim_in_path(dirs, "some/default/path");
        EXPECT_EQ(dirs[0], "to/test");
        EXPECT_EQ(dirs[1], "another/test");
        EXPECT_EQ(dirs[2], "last");
        EXPECT_EQ(Files::trim_path("path/to/test", "no_match/to/test"), "path/to/test");
        EXPECT_EQ(Files::trim_path("path/to/test", "to/"), "test");
        EXPECT_EQ(Files::trim_path("", "to/"), "");
        EXPECT_EQ(Files::trim_path("test", ""), "test");
        EXPECT_EQ(Files::trim_path("test", "/"), "test");
        EXPECT_EQ(Files::trim_path("test", "test"), "");

        EXPECT_EQ(Files::normalize("/path/to/the/../test"), "/path/to/test");
        EXPECT_EQ(Files::normalize("path/to/the/../test"), "path/to/test");
        EXPECT_EQ(Files::normalize("path///to//..///..///test"), "test");
        EXPECT_EQ(Files::normalize("path/to/..///../..///test"), "../test");
        EXPECT_EQ(Files::normalize("path/.."), "");
        EXPECT_EQ(Files::normalize("/path/.."), "/");
        EXPECT_EQ(Files::normalize(".."), "..");
        EXPECT_EQ(Files::normalize(""), "");

        Files::sep = '\\';
        EXPECT_EQ(Files::get_filename("\\path\\to\\test.txt"), "test.txt");
        EXPECT_EQ(Files::get_parent("\\path\\to\\test.txt"), "\\path\\to");
    }

    TEST_F(TestFiles, test_files_creation)
    {
        EXPECT_TRUE(Files::exists(_test_path));
        EXPECT_TRUE(Files::is_dir(_test_path));
        EXPECT_FALSE(Files::is_file(_test_path));
        std::string sandbox_path = Files::combine({_base_test_dir, "creation"});
        EXPECT_TRUE(this->log_make_dirs(sandbox_path));
        EXPECT_TRUE(Files::exists(sandbox_path));
        EXPECT_TRUE(Files::is_dir(sandbox_path));
        EXPECT_FALSE(Files::is_file(sandbox_path));
        EXPECT_TRUE(this->log_make_dirs(sandbox_path));

        std::string path_to_test = Files::combine({sandbox_path, "path", "to", "test"});
        EXPECT_TRUE(this->log_make_dirs(path_to_test));
        EXPECT_TRUE(Files::exists(path_to_test));
        EXPECT_TRUE(Files::is_dir(path_to_test));
        std::ofstream file1(Files::combine({sandbox_path, "path", "file1.txt"}));
        std::ofstream file2(Files::combine({sandbox_path, "path", "to", "file2.txt"}));
        std::ofstream file3(Files::combine({sandbox_path, "path", "to", "test", "file3.txt"}));
        EXPECT_TRUE(file1.is_open());
        EXPECT_TRUE(file2.is_open());
        EXPECT_TRUE(file3.is_open());

        std::vector<std::string> rec_children = Files::get_recursive_children(sandbox_path);
        for (const auto & child: rec_children)
        {
            LOG(debug, "get_recursive_children: " << child);
        }
        EXPECT_EQ(rec_children.size(), 6u);

        std::vector<std::string> children = Files::get_children(Files::combine(sandbox_path, "path"));
        for (const auto & child: children)
        {
            LOG(debug, "get_children: " << child);
        }
        EXPECT_EQ(children.size(), 2u);

        std::string dirname = Files::combine(sandbox_path, "random_dir");
        EXPECT_FALSE(Files::is_dir(dirname));
        EXPECT_TRUE(log_make_dirs(dirname));
        EXPECT_TRUE(Files::is_dir(dirname));
        EXPECT_TRUE(Files::remove_directory(dirname));
        EXPECT_FALSE(Files::is_dir(dirname));

        EXPECT_TRUE(Files::remove_directories(sandbox_path));
        EXPECT_FALSE(Files::is_dir(path_to_test));
        EXPECT_TRUE(Files::is_dir(sandbox_path));
    }

    TEST_F(TestFiles, test_files_fast_io)
    {
        std::string file_content = "hello world\n";
        std::string path = Files::combine({_base_test_dir, "io", "test.txt"});
        EXPECT_TRUE(Str::ends_with(path, "test/util_files/io/test.txt"));
        EXPECT_TRUE(this->log_make_dirs(Files::get_parent(path)));
        LOG(info, "Writing file to: " << path);
        EXPECT_TRUE(Files::write(path, file_content));
        EXPECT_EQ(Files::read_all(path), file_content);
        EXPECT_EQ(Files::read(path, 5), "hello");
        char data[10];
        EXPECT_EQ(Files::read_binary(path, data, 5), 5);
        data[5] = 0;
        EXPECT_STREQ(data, "hello");

        EXPECT_EQ(Files::read_all("/there/is/no/path.txt"), std::nullopt);
        EXPECT_FALSE(Files::write("/there/is/no/path.txt", "none"));
        EXPECT_EQ(Files::read_all("/there/is/no/path.txt"), std::nullopt);
    }

}
