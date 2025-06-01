#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/num.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestFS: public ::testing::Test
{
    protected:
        TestFS() { sihd::util::LoggerManager::stream(); }

        virtual ~TestFS() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() { fs::set_sep(this->original_slash); }

        bool log_make_dirs(std::string path)
        {
            SIHD_LOG(debug, "Making directory: {}", path);
            return std::filesystem::create_directories(path);
        }

        char original_slash = fs::sep();
};

TEST_F(TestFS, test_fs_path)
{
    fs::set_sep('/');
    EXPECT_EQ(fs::combine("path", "test"), "path/test");
    EXPECT_EQ(fs::combine("path/to", "test"), "path/to/test");
    EXPECT_EQ(fs::combine("/path/to", "test"), "/path/to/test");
    EXPECT_EQ(fs::combine("/path/to/", "test"), "/path/to/test");
    EXPECT_EQ(fs::combine("/path/to//", "test"), "/path/to//test");
    EXPECT_EQ(fs::combine("/path/to/", "/test"), "/path/to//test");
    EXPECT_EQ(fs::combine("/path/to", "/test"), "/path/to//test");
    EXPECT_EQ(fs::combine("/", "test"), "/test");
    EXPECT_EQ(fs::combine("", "test"), "test");
    EXPECT_EQ(fs::combine("test", ""), "test/");

    EXPECT_EQ(fs::combine({"path", "to", "test"}), "path/to/test");

    EXPECT_EQ(fs::extension("/path/to/test.txt"), "txt");
    EXPECT_EQ(fs::extension("/path/to/test"), "");
    EXPECT_EQ(fs::extension(""), "");
    EXPECT_EQ(fs::extension("archive.tar.gz"), "tar.gz");
    EXPECT_EQ(fs::extension("/path/to/archive.tar.gz"), "tar.gz");

    EXPECT_EQ(fs::filename("/path/to/test.txt"), "test.txt");
    EXPECT_EQ(fs::filename("/path/to/////test.txt"), "test.txt");
    EXPECT_EQ(fs::filename("test.txt"), "test.txt");
    EXPECT_EQ(fs::filename(""), "");

    EXPECT_EQ(fs::parent("/path/to/test.txt"), "/path/to");
    EXPECT_EQ(fs::parent("/path/to////test.txt"), "/path/to");
    EXPECT_EQ(fs::parent("/path/to"), "/path");
    EXPECT_EQ(fs::parent("/path/to/"), "/path");
    EXPECT_EQ(fs::parent("/path/to////"), "/path");
    EXPECT_EQ(fs::parent("/path"), "");
    EXPECT_EQ(fs::parent("////path"), "");
    EXPECT_EQ(fs::parent("////path//"), "");
    EXPECT_EQ(fs::parent("/"), "");
    EXPECT_EQ(fs::parent("//////"), "");
    EXPECT_EQ(fs::parent("filename.txt"), "");
    EXPECT_EQ(fs::parent(""), "");

    EXPECT_EQ(fs::trim_path("/path/to/test", "/path/to"), "test");
    EXPECT_EQ(fs::trim_path("/path/to/test", "/path/to"), "test");
    std::string test("/path/to/test");
    EXPECT_EQ(fs::trim_path(test, "/path/"), "to/test");
    fs::trim_in_path(test, "/path");
    EXPECT_EQ(test, "to/test");
    std::vector<std::string> dirs = {
        "some/default/path/to/test",
        "some/default/path/another/test",
        "some/default/path/last",
    };
    fs::trim_in_path(dirs, "some/default/path");
    EXPECT_EQ(dirs[0], "to/test");
    EXPECT_EQ(dirs[1], "another/test");
    EXPECT_EQ(dirs[2], "last");
    EXPECT_EQ(fs::trim_path("path/to/test", "no_match/to/test"), "path/to/test");
    EXPECT_EQ(fs::trim_path("path/to/test", "to/"), "test");
    EXPECT_EQ(fs::trim_path("", "to/"), "");
    EXPECT_EQ(fs::trim_path("test", ""), "test");
    EXPECT_EQ(fs::trim_path("test", "/"), "test");
    EXPECT_EQ(fs::trim_path("test", "test"), "");

    EXPECT_EQ(fs::normalize("/path/to/the/../test"), "/path/to/test");
    EXPECT_EQ(fs::normalize("path/to/the/../test"), "path/to/test");
    EXPECT_EQ(fs::normalize("path///to//..///..///test"), "test");
    EXPECT_EQ(fs::normalize("path/to/..///../..///test"), "../test");
    EXPECT_EQ(fs::normalize("path/.."), "");
    EXPECT_EQ(fs::normalize("/path/.."), "/");
    EXPECT_EQ(fs::normalize(".."), "..");
    EXPECT_EQ(fs::normalize(""), "");

    fs::set_sep('\\');
    EXPECT_EQ(fs::filename("\\path\\to\\test.txt"), "test.txt");
    EXPECT_EQ(fs::parent("\\path\\to\\test.txt"), "\\path\\to");
}

TEST_F(TestFS, test_fs_creation)
{
    auto tmp_path = std::filesystem::temp_directory_path() / str::to_hex(num::rand());
    ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
    auto tmp_path_str = tmp_path.string();

    EXPECT_TRUE(fs::exists(tmp_path_str));
    EXPECT_TRUE(fs::is_dir(tmp_path_str));
    EXPECT_FALSE(fs::is_file(tmp_path_str));
    std::string sandbox_path = fs::combine({tmp_path_str, "creation"});
    EXPECT_TRUE(this->log_make_dirs(sandbox_path));
    EXPECT_TRUE(fs::exists(sandbox_path));
    EXPECT_TRUE(fs::is_dir(sandbox_path));
    EXPECT_FALSE(fs::is_file(sandbox_path));

    std::string path_to_test = fs::combine({sandbox_path, "path", "to", "test"});
    EXPECT_TRUE(this->log_make_dirs(path_to_test));
    EXPECT_TRUE(fs::exists(path_to_test));
    EXPECT_TRUE(fs::is_dir(path_to_test));
    std::ofstream file1(fs::combine({sandbox_path, "path", "file1.txt"}));
    std::ofstream file2(fs::combine({sandbox_path, "path", "to", "file2.txt"}));
    std::ofstream file3(fs::combine({sandbox_path, "path", "to", "test", "file3.txt"}));
    EXPECT_TRUE(file1.is_open());
    EXPECT_TRUE(file2.is_open());
    EXPECT_TRUE(file3.is_open());

    std::vector<std::string> rec_children = fs::recursive_children(sandbox_path);
    for (const auto & child : rec_children)
    {
        SIHD_LOG(debug, "recursive_children: {}", child);
    }
    EXPECT_EQ(rec_children.size(), 6u);

    std::vector<std::string> children = fs::children(fs::combine(sandbox_path, "path"));
    for (const auto & child : children)
    {
        SIHD_LOG(debug, "children: {}", child);
    }
    EXPECT_EQ(children.size(), 2u);

    std::string dirname = fs::combine(sandbox_path, "random_dir");
    EXPECT_FALSE(fs::is_dir(dirname));
    EXPECT_TRUE(log_make_dirs(dirname));
    EXPECT_TRUE(fs::is_dir(dirname));
    EXPECT_TRUE(fs::remove_directory(dirname));
    EXPECT_TRUE(fs::make_directory(dirname));
    EXPECT_TRUE(fs::remove_directory(dirname));
    EXPECT_FALSE(fs::is_dir(dirname));

    EXPECT_TRUE(fs::remove_directories(sandbox_path));
    EXPECT_FALSE(fs::is_dir(path_to_test));
    EXPECT_TRUE(fs::is_dir(sandbox_path));
}

TEST_F(TestFS, test_fs_fast_io)
{
    auto tmp_path = std::filesystem::temp_directory_path() / str::to_hex(num::rand());
    ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
    std::string path = fs::combine({tmp_path.string(), "io", "test.txt"});

    std::string file_content = "hello world\n";
    EXPECT_TRUE(str::ends_with(path, "/io/test.txt"));
    EXPECT_TRUE(this->log_make_dirs(fs::parent(path)));
    SIHD_LOG(info, "Writing file to: {}", path);
    EXPECT_TRUE(fs::write(path, file_content));
    EXPECT_EQ(fs::read_all(path), file_content);
    EXPECT_EQ(fs::read(path, 5), "hello");
    char data[10];
    EXPECT_EQ(fs::read_binary(path, data, 5), 5);
    data[5] = 0;
    EXPECT_STREQ(data, "hello");

    EXPECT_EQ(fs::read_all("/there/is/no/path.txt"), std::nullopt);
    EXPECT_FALSE(fs::write("/there/is/no/path.txt", "none"));
    EXPECT_EQ(fs::read_all("/there/is/no/path.txt"), std::nullopt);
}

TEST_F(TestFS, test_fs_permission)
{
    auto tmp_path = std::filesystem::temp_directory_path() / str::to_hex(num::rand());
    ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
    std::string path = fs::combine({tmp_path.string(), "permission"});
    std::ofstream ofs(path);
    ofs << "\n";
    ofs.close();
    EXPECT_TRUE(fs::is_readable(path));
    EXPECT_TRUE(fs::is_writable(path));
    EXPECT_FALSE(fs::is_executable(path));

    EXPECT_TRUE(fs::permission_set(path, 0700));
    EXPECT_TRUE(fs::is_executable(path));
    EXPECT_EQ(fs::permission_get(path), 0700U);

    EXPECT_TRUE(fs::permission_rm(path, fs::permission_from_str("r--")));
    EXPECT_EQ(fs::permission_get(path), 0300U);
    EXPECT_TRUE(fs::permission_add(path, fs::permission_from_str("---r---w-")));
    EXPECT_EQ(fs::permission_get(path), 0342U);

    EXPECT_EQ(fs::permission_to_str(0750), "rwxr-x---");
    EXPECT_EQ(fs::permission_from_str("rwxr-x"), 0750U);

    EXPECT_TRUE(fs::permission_set(path, fs::permission_from_str("rwxr-x-w-")));
    EXPECT_EQ(fs::permission_get(path), 0752U);
}

} // namespace test
