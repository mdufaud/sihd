#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include "test_helper.hpp"

#if defined(__SIHD_WINDOWS__)
# include <windows.h>
#endif

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

#if defined(__SIHD_WINDOWS__)
bool running_under_wine()
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}
#else
bool running_under_wine()
{
    return false;
}
#endif

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
    EXPECT_EQ(fs::filename("/path/to///..///test.txt"), "test.txt");
    EXPECT_EQ(fs::filename("test.txt"), "test.txt");
    EXPECT_EQ(fs::filename(""), "");

    EXPECT_EQ(fs::parent("/path/to/test.txt"), "/path/to");
    EXPECT_EQ(fs::parent("/path/to////test.txt"), "/path/to");
    EXPECT_EQ(fs::parent("/path/to"), "/path");
    EXPECT_EQ(fs::parent("/path/to/.."), "");
    EXPECT_EQ(fs::parent("/path/to/test/.."), "/path");
    EXPECT_EQ(fs::parent("/path/to/"), "/path");
    EXPECT_EQ(fs::parent("/path/to////"), "/path");
    EXPECT_EQ(fs::parent("/path"), "");
    EXPECT_EQ(fs::parent("////path"), "");
    EXPECT_EQ(fs::parent("////path//"), "");
    EXPECT_EQ(fs::parent("/"), "");
    EXPECT_EQ(fs::parent("//////"), "");
    EXPECT_EQ(fs::parent("filename.txt"), "");
    EXPECT_EQ(fs::parent("path/to/..///../..///test"), "..");
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

TEST_F(TestFS, test_fs_jail)
{
    fs::set_sep('/');

    // Empty root: no jail, path returned unchanged
    EXPECT_EQ(fs::jail("", "any/../path"), "any/../path");

    // Use a nonexistent root so realpath fails and jail returns the
    // lexically-jailed candidate (deterministic, filesystem-independent)
    EXPECT_EQ(fs::jail("/srv/ftp", "file.txt"), "/srv/ftp/file.txt");
    EXPECT_EQ(fs::jail("/srv/ftp", "sub/dir/file.txt"), "/srv/ftp/sub/dir/file.txt");

    // Leading '/' on the client path is treated as relative to the jail root
    EXPECT_EQ(fs::jail("/srv/ftp", "/file.txt"), "/srv/ftp/file.txt");

    // Trailing slash on root is normalized away
    EXPECT_EQ(fs::jail("/srv/ftp/", "file.txt"), "/srv/ftp/file.txt");

    // Internal '..' that stays inside the jail is collapsed
    EXPECT_EQ(fs::jail("/srv/ftp", "sub/../file.txt"), "/srv/ftp/file.txt");

    // Escape attempts clamp to the jail root
    EXPECT_EQ(fs::jail("/srv/ftp", "../../etc/passwd"), "/srv/ftp");
    EXPECT_EQ(fs::jail("/srv/ftp", "../ftp_evil"), "/srv/ftp");
    EXPECT_EQ(fs::jail("/srv/ftp", "/../etc/passwd"), "/srv/ftp");

#if !defined(__SIHD_WINDOWS__)
    // symlink creation + realpath resolution are POSIX-specific (windows symlinks need privilege)
    // A symlink inside the jail that points outside must be clamped to root
    TmpDir root_dir;
    ASSERT_TRUE(static_cast<bool>(root_dir));
    const std::string & root = root_dir.path();
    ASSERT_TRUE(fs::make_file_link("/etc/passwd", fs::combine(root, "escape")));
    EXPECT_EQ(fs::jail(root, "escape"), root);

    // A real file inside the jail resolves to itself
    const std::string inside = fs::combine(root, "inside.txt");
    ASSERT_TRUE(fs::write(inside, "data"));
    EXPECT_EQ(fs::jail(root, "inside.txt"), fs::realpath(inside));
#endif
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
    // windows cannot delete a still-open file -> close before remove_directories
    file1.close();
    file2.close();
    file3.close();

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

#if !defined(__SIHD_WINDOWS__)
    const std::string link_target = fs::combine({sandbox_path, "path", "file1.txt"});
    const std::string link_path = fs::combine({sandbox_path, "path", "file_link"});
    EXPECT_TRUE(fs::make_file_link(link_target, link_path));
    EXPECT_TRUE(fs::is_symlink(link_path));
#endif

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
    EXPECT_TRUE(str::ends_with(path, fs::combine("io", "test.txt")));
    EXPECT_TRUE(this->log_make_dirs(fs::parent(path)));
    SIHD_LOG(info, "Writing file to: {}", path);
    EXPECT_TRUE(fs::write(path, file_content));
    EXPECT_EQ(fs::read_all(path).value_or(""), file_content);
    EXPECT_EQ(fs::read(path, {0, 4}).value_or(""), "hello");
    char data[10];
    EXPECT_EQ(fs::read_binary(path, data, 5), 5);
    data[5] = 0;
    EXPECT_STREQ(data, "hello");

    std::string content_z = "bin\032ary";
    EXPECT_TRUE(fs::write(path, content_z, false, true));
    EXPECT_EQ(fs::read_all(path, true).value_or(""), content_z);
#if defined(__SIHD_WINDOWS__)
    // the default text mode stops at ^Z
    EXPECT_EQ(fs::read_all(path).value_or(""), "bin");
#endif

    EXPECT_EQ(fs::read_all("/there/is/no/path.txt"), std::nullopt);
    EXPECT_FALSE(fs::write("/there/is/no/path.txt", "none"));
    EXPECT_EQ(fs::read_all("/there/is/no/path.txt"), std::nullopt);
}

TEST_F(TestFS, test_fs_read_lines)
{
    auto tmp_path = std::filesystem::temp_directory_path() / str::to_hex(num::rand());
    ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
    std::string path = fs::combine({tmp_path.string(), "lines.txt"});

    EXPECT_FALSE(fs::read_lines(path).has_value());

    ASSERT_TRUE(fs::write(path, "first\n\nsecond\n"));
    const auto lines = fs::read_lines(path);
    ASSERT_TRUE(lines.has_value());
    ASSERT_EQ(lines->size(), 2u);
    EXPECT_EQ((*lines)[0], "first");
    EXPECT_EQ((*lines)[1], "second");
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

#if !defined(__SIHD_WINDOWS__)
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
#else
    // windows only models the read-only attribute (no rwx/owner-group-other bits)
    EXPECT_TRUE(fs::permission_set(path, fs::permission_from_str("r--r--r--")));
    EXPECT_FALSE(fs::is_writable(path));
    EXPECT_TRUE(fs::permission_set(path, fs::permission_from_str("rw-rw-rw-")));
    EXPECT_TRUE(fs::is_writable(path));
#endif
}

TEST_F(TestFS, test_fs_mount_type)
{
    // the build tree lives on a real local disk on every CI we run
    EXPECT_EQ(fs::mount_type(fs::cwd()), fs::MountType::local);

    // unresolvable paths never crash, report unknown
    EXPECT_EQ(fs::mount_type(""), fs::MountType::unknown);
    EXPECT_EQ(fs::mount_type("/nonexistent/zzz"), fs::MountType::unknown);

    // storage_medium contract: no rotational backing for these kinds
    const fs::MountType type = fs::mount_type(fs::cwd());
    if (type == fs::MountType::network || type == fs::MountType::ram || type == fs::MountType::readonly)
    {
        EXPECT_EQ(fs::storage_medium(fs::cwd()), fs::StorageMedium::unknown);
    }

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
    // /dev/shm is tmpfs on a normal Linux host (unprivileged); guard the rare host without it
    if (fs::is_dir("/dev/shm"))
    {
        EXPECT_EQ(fs::mount_type("/dev/shm"), fs::MountType::ram);
        EXPECT_EQ(fs::storage_medium("/dev/shm"), fs::StorageMedium::unknown);
    }
#endif

#if defined(__SIHD_WINDOWS__)
    EXPECT_EQ(fs::mount_type("C:\\"), fs::MountType::local);
    // pure path-prefix branch, no real share needed
    EXPECT_EQ(fs::mount_type("\\\\nonexistent\\share"), fs::MountType::network);
#endif
}

TEST_F(TestFS, test_fs_storage_medium)
{
    // documents the enum, tolerates sandboxed sysfs / unresolvable backings
    const fs::StorageMedium medium = fs::storage_medium(fs::cwd());
    EXPECT_TRUE(medium == fs::StorageMedium::ssd || medium == fs::StorageMedium::hdd
                || medium == fs::StorageMedium::unknown);
}

TEST_F(TestFS, test_fs_disk_space)
{
    const auto free_bytes = fs::free_space(fs::cwd());
    const auto total_bytes = fs::total_space(fs::cwd());
    ASSERT_TRUE(free_bytes.has_value());
    ASSERT_TRUE(total_bytes.has_value());
    EXPECT_GT(*total_bytes, 0u);
    EXPECT_LE(*free_bytes, *total_bytes);
    SIHD_LOG(debug, "disk: {} / {} bytes free", *free_bytes, *total_bytes);

    EXPECT_EQ(fs::free_space("/nonexistent/zzz"), std::nullopt);
    EXPECT_EQ(fs::total_space("/nonexistent/zzz"), std::nullopt);
}

TEST_F(TestFS, test_fs_mounts)
{
#if defined(__SIHD_EMSCRIPTEN__)
    GTEST_SKIP() << "no /proc/mounts on emscripten";
#else
    const std::vector<fs::MountEntry> mounts = fs::mounts();
    ASSERT_FALSE(mounts.empty());
# if defined(__SIHD_WINDOWS__)
    const std::string expected_root = "C:\\";
# else
    const std::string expected_root = "/";
# endif
    bool found_root = false;
    bool found_local = false;
    for (const auto & mount : mounts)
    {
        SIHD_LOG(debug, "mount: {} on {} ({})", mount.source, mount.mount_point, mount.fs_type);
        if (mount.mount_point == expected_root)
            found_root = true;
        if (mount.type == fs::MountType::local)
            found_local = true;
    }
    EXPECT_TRUE(found_root);
    EXPECT_TRUE(found_local);
# if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
    // pseudo filesystems have no storage backing (paths exist under wine's Z:;
    // /dev can be devtmpfs or tmpfs depending on the setup)
    EXPECT_EQ(fs::mount_type("/proc"), fs::MountType::unknown);
    EXPECT_EQ(fs::mount_type("/sys"), fs::MountType::unknown);
# endif
#endif
}

TEST_F(TestFS, test_fs_times)
{
    auto tmp_path = std::filesystem::temp_directory_path() / str::to_hex(num::rand());
    ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
    std::string path = fs::combine({tmp_path.string(), "times.txt"});

    EXPECT_FALSE(fs::times(path).has_value());

    ASSERT_TRUE(fs::write(path, "times"));
    const auto times = fs::times(path);
    ASSERT_TRUE(times.has_value());
    EXPECT_NE(times->write.get(), 0);
    EXPECT_NE(times->access.get(), 0);
#if defined(__SIHD_WINDOWS__)
    EXPECT_NE(times->creation.get(), 0);
#endif
    if (times->creation.get() != 0)
    {
        EXPECT_LE(times->creation.get(), times->write.get());
    }

    // timestamps have a coarse granularity on some filesystems
    time::msleep(20);
    ASSERT_TRUE(fs::write(path, "times again"));
    const auto times2 = fs::times(path);
    ASSERT_TRUE(times2.has_value());
    EXPECT_GT(times2->write.get(), times->write.get());
    EXPECT_EQ(fs::last_write(path).get(), times2->write.get());
}

TEST_F(TestFS, test_fs_copy_file)
{
    auto tmp_path = std::filesystem::temp_directory_path() / str::to_hex(num::rand());
    ASSERT_TRUE(std::filesystem::create_directory(tmp_path));
    const std::string src = fs::combine({tmp_path.string(), "copy_src.bin"});
    const std::string dst = fs::combine({tmp_path.string(), "copy_dst.bin"});
    const std::string dst_cancel = fs::combine({tmp_path.string(), "copy_cancel.bin"});
    const std::string same = fs::combine({tmp_path.string(), "copy_same.bin"});

    std::string content(5 * 1024 * 1024, '\0');
    for (size_t i = 0; i < content.size(); ++i)
        content[i] = (char)(i * 31 % 251);
    ASSERT_TRUE(fs::write(src, content, false, true));

    // Wine does not implement the CopyFile2 progress callback ("PCOPYFILE2_PROGRESS_ROUTINE
    // is not supported" in its kernelbase/file.c): the callback never runs there, so
    // progress counting and cancellation cannot be verified
    size_t transferred = 0;
    EXPECT_TRUE(fs::copy_file(src, dst, [&](size_t progress, size_t total) {
        EXPECT_EQ(total, content.size());
        EXPECT_GE(progress, transferred);
        transferred = progress;
        return true;
    }));
    EXPECT_TRUE(fs::are_equals(src, dst));
    if (!running_under_wine())
    {
        EXPECT_EQ(transferred, content.size());
    }
    // the copy preserves the source write time (CopyFile2 / futimens)
    const auto src_times = fs::times(src);
    const auto dst_times = fs::times(dst);
    ASSERT_TRUE(src_times.has_value());
    ASSERT_TRUE(dst_times.has_value());
    EXPECT_EQ(dst_times->write.get(), src_times->write.get());

    // overwrite without progress callback
    EXPECT_TRUE(fs::copy_file(src, dst));
    EXPECT_TRUE(fs::are_equals(src, dst));

    // copying a file onto itself is refused and keeps it intact
    ASSERT_TRUE(fs::copy_file(src, same));
    EXPECT_FALSE(fs::copy_file(same, same));
    EXPECT_EQ(fs::file_size(same).value_or(0), content.size());

    if (!running_under_wine())
    {
        // cancellation removes the partial destination
        bool cancelled = false;
        EXPECT_FALSE(fs::copy_file(src, dst_cancel, [&](size_t progress, size_t total) {
            if (progress >= total / 2)
            {
                cancelled = true;
                return false;
            }
            return true;
        }));
        EXPECT_TRUE(cancelled);
        EXPECT_FALSE(fs::is_file(dst_cancel));
    }

    // empty files copy fine
    const std::string empty_src = fs::combine({tmp_path.string(), "empty_src.bin"});
    const std::string empty_dst = fs::combine({tmp_path.string(), "empty_dst.bin"});
    ASSERT_TRUE(fs::write(empty_src, ""));
    EXPECT_TRUE(fs::copy_file(empty_src, empty_dst));
    EXPECT_TRUE(fs::are_equals(empty_src, empty_dst));

    // missing source
    EXPECT_FALSE(fs::copy_file(fs::combine({tmp_path.string(), "nope.bin"}), dst));
}

TEST_F(TestFS, test_fs_platform_paths)
{
    for (const std::string & path : {fs::config_path(), fs::data_path(), fs::cache_path(), fs::download_path()})
    {
        EXPECT_FALSE(path.empty()) << path;
        EXPECT_TRUE(fs::is_absolute(path)) << path;
    }
    EXPECT_NE(fs::config_path(), fs::cache_path());
}

#if !defined(__SIHD_WINDOWS__)

TEST_F(TestFS, test_fs_xdg_paths)
{
    const std::string override_dir = fs::combine(fs::tmp_path(), "sihd_test_xdg");

    {
        ScopedEnv cache("XDG_CACHE_HOME", override_dir);
        EXPECT_EQ(fs::cache_path(), override_dir);
    }

    // a relative value is ignored per the spec
    {
        ScopedEnv data("XDG_DATA_HOME", "relative/path");
        EXPECT_TRUE(str::ends_with(fs::data_path(), "/.local/share"));
    }

    // no HOME means no default under home
    {
        ScopedEnv no_home("HOME", std::nullopt);
        ScopedEnv no_config("XDG_CONFIG_HOME", std::nullopt);
        EXPECT_EQ(fs::config_path(), "");
    }
}

TEST_F(TestFS, test_fs_download_path)
{
    // destructor removes the directory even when an assertion aborts the test
    TmpDir tmp_config;
    ASSERT_TRUE(tmp_config);
    const std::string dirs_file = fs::combine(tmp_config.path(), "user-dirs.dirs");
    const std::string fallback = fs::combine(fs::home_path(), "Downloads");

    ScopedEnv config("XDG_CONFIG_HOME", tmp_config.path());

    // no user-dirs.dirs: falls back to ~/Downloads
    EXPECT_EQ(fs::download_path(), fallback);

    // $HOME is expanded
    ASSERT_TRUE(fs::write(dirs_file, "XDG_DOWNLOAD_DIR=\"$HOME/Telechargements\"\n"));
    EXPECT_EQ(fs::download_path(), fs::combine(fs::home_path(), "Telechargements"));

    // absolute values are used as-is
    ASSERT_TRUE(fs::write(dirs_file, "XDG_DOWNLOAD_DIR=\"/data/downloads\"\n"));
    EXPECT_EQ(fs::download_path(), "/data/downloads");

    // an unusable value falls back to ~/Downloads
    ASSERT_TRUE(fs::write(dirs_file, "XDG_DOWNLOAD_DIR=\"relative/dir\"\n"));
    EXPECT_EQ(fs::download_path(), fallback);

    // other keys are ignored
    ASSERT_TRUE(fs::write(dirs_file, "XDG_MUSIC_DIR=\"$HOME/Music\"\n"));
    EXPECT_EQ(fs::download_path(), fallback);
}

#endif

} // namespace test
