#include <dirent.h> // DIR...
#include <sys/stat.h>
#include <unistd.h> // access

#include <cstdio>
#include <cstring> // strcmp
#include <filesystem>
#include <fstream>

#include <fmt/ranges.h>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <linux/magic.h>   // *_SUPER_MAGIC
# include <sys/statfs.h>    // statfs
# include <sys/sysmacros.h> // major / minor
#endif

namespace sihd::sys::fs
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::fs");

namespace
{

bool do_stat(std::string_view path, struct stat *s)
{
    return ::stat(path.data(), s) == 0;
}

void get_recursive_children(std::string_view path,
                            std::vector<std::string> & children,
                            uint32_t current_depth,
                            uint32_t max_depth)
{
    if (max_depth > 0 && current_depth >= max_depth)
        return;

    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.data())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue;
            std::string childpath = combine(path, dirent->d_name);
            if (dirent->d_type & DT_DIR)
            {
                children.push_back(childpath + sep_str());
                get_recursive_children(childpath, children, current_depth + 1, max_depth);
            }
            else
            {
                children.push_back(childpath);
            }
        }
        closedir(dir_ptr);
    }
}

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

StorageMedium read_rotational(const std::string & sysfs_dir)
{
    std::ifstream file(sysfs_dir + "/queue/rotational");
    char c;
    if (file.is_open() && file.get(c))
        return c == '1' ? StorageMedium::hdd : StorageMedium::ssd;
    return StorageMedium::unknown;
}

StorageMedium storage_medium_from_devnum(dev_t st_dev)
{
    // /sys/dev/block/<major>:<minor> symlinks to the block device's sysfs dir.
    // A whole disk exposes queue/rotational directly; a partition does not, but
    // its parent disk (..) does (best-effort for dm/LVM via the same parent walk).
    const std::string base = fmt::format("/sys/dev/block/{}:{}", major(st_dev), minor(st_dev));
    StorageMedium ret = read_rotational(base);
    if (ret == StorageMedium::unknown)
        ret = read_rotational(base + "/..");
    return ret;
}

#endif

} // namespace

std::string home_path()
{
    return getenv("HOME");
}

std::string executable_path()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return "";
#else
    std::string path;
    try
    {
        path = std::filesystem::canonical("/proc/self/exe");
        if (path.empty() == false)
            return path;
    }
    catch ([[maybe_unused]] const std::filesystem::filesystem_error & e)
    {
    }
    std::ifstream mapf("/proc/self/maps");
    std::string line;
    if (std::getline(mapf, line))
    {
        size_t idx = line.find("/");
        if (idx != std::string::npos)
        {
            path = line.substr(idx);
            return path;
        }
    }
#endif
    return ".";
}

// stat

bool exists(std::string_view path)
{
    return access(path.data(), F_OK) == 0;
}

bool is_readable(std::string_view path)
{
    return access(path.data(), R_OK) == 0;
}

bool is_writable(std::string_view path)
{
    return access(path.data(), W_OK) == 0;
}

bool is_executable(std::string_view path)
{
    return access(path.data(), X_OK) == 0;
}

Timestamp last_write(std::string_view path)
{
    struct stat s;
    return do_stat(path.data(), &s) ? Timestamp(time::seconds(s.st_mtime)) : Timestamp {};
}

std::optional<size_t> file_size(std::string_view path)
{
    struct stat s;
    return do_stat(path.data(), &s) ? s.st_size : std::optional<size_t> {};
}

// directories

std::string tmp_path()
{
    const char *tmp_path;

    (tmp_path = getenv("TMPDIR")) || (tmp_path = getenv("TMP")) || (tmp_path = getenv("TEMP"))
        || (tmp_path = getenv("TMPDIR"));
    return tmp_path != nullptr ? tmp_path : "/tmp";
}

std::string make_tmp_directory(std::string_view prefix)
{
    if (prefix.size() + 6 > PATH_MAX)
        throw std::runtime_error(fmt::format("make_tmp_directory: path too long: {}", prefix));

    std::string path;
    path.reserve(prefix.size() + 6 + 1);
    path += prefix;
    path += "XXXXXX";
    if (mkdtemp(path.data()) != nullptr)
        return path;
    return "";
}

bool make_directory(std::string_view path, unsigned int mode)
{
    if (is_dir(path))
        return true;
    if (path.empty())
        return false;
    return mkdir(path.data(), mode) == 0;
}

std::vector<std::string> recursive_children(std::string_view path, uint32_t max_depth)
{
    std::vector<std::string> ret;
    uint32_t current_depth = 0;
    get_recursive_children(path, ret, current_depth, max_depth);
    return ret;
}

std::vector<std::string> children(std::string_view path)
{
    std::vector<std::string> ret;
    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.data())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue;
            if (dirent->d_type & DT_DIR)
                ret.push_back(std::string(dirent->d_name) + sep_str());
            else
                ret.push_back(dirent->d_name);
        }
        closedir(dir_ptr);
    }
    return ret;
}

// files

bool truncate(std::string_view path, int64_t size)
{
    return ::truncate(path.data(), static_cast<off_t>(size)) == 0;
}

std::string realpath(std::string_view path)
{
    char *real = ::realpath(path.data(), nullptr);
    if (!real)
        return "";
    std::string result(real);
    free(real);
    return result;
}

bool chdir(std::string_view path)
{
    return ::chdir(path.data()) == 0;
}

MountType mount_type([[maybe_unused]] std::string_view path)
{
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
    struct statfs buf;
    if (::statfs(path.data(), &buf) != 0)
        return MountType::unknown;

    switch (static_cast<unsigned long>(buf.f_type))
    {
        case NFS_SUPER_MAGIC:
        case SMB_SUPER_MAGIC:
        case 0xFF534D42UL: // CIFS_MAGIC_NUMBER
        case 0xFE534D42UL: // SMB2_MAGIC_NUMBER
        case V9FS_MAGIC:
            return MountType::network;
        case TMPFS_MAGIC:
            return MountType::ram;
        case SQUASHFS_MAGIC:
            return MountType::readonly;
        default:
            // FUSE-based remotes (sshfs) report FUSE_SUPER_MAGIC and fall here as
            // local: telling them apart needs parsing the mount source.
            return MountType::local;
    }
#else
    return MountType::unknown;
#endif
}

StorageMedium storage_medium([[maybe_unused]] std::string_view path)
{
    // no rotational backing for these (or backing is a loop file)
    const MountType type = mount_type(path);
    if (type == MountType::network || type == MountType::ram || type == MountType::readonly)
        return StorageMedium::unknown;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
    struct stat s;
    if (!do_stat(path, &s))
        return StorageMedium::unknown;
    return storage_medium_from_devnum(s.st_dev);
#else
    return StorageMedium::unknown;
#endif
}

} // namespace sihd::sys::fs
