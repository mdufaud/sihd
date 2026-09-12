#include <dirent.h> // DIR...
#include <fcntl.h>  // open, AT_FDCWD
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h> // access

#if defined(__linux__)
# include <sys/sendfile.h> // sendfile
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring> // strcmp
#include <filesystem>
#include <fstream>

#include <fmt/ranges.h>

#include <sihd/sys/env.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/str.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <sys/statfs.h>    // statfs
# include <sys/sysmacros.h> // major / minor

# include <linux/magic.h> // *_SUPER_MAGIC
#endif

// statx is declared by glibc >= 2.28 and musl >= 1.1.24; bionic/emscripten lack it
#if defined(__linux__) && defined(STATX_BTIME) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define SIHD_FS_HAS_STATX
#endif

// newer kernel headers lack the newer pseudo-filesystem magic numbers
#ifndef DEVTMPFS_MAGIC
# define DEVTMPFS_MAGIC 0x45858651
#endif
#ifndef MQUEUE_MAGIC
# define MQUEUE_MAGIC 0x19800202
#endif
#ifndef CONFIGFS_MAGIC
# define CONFIGFS_MAGIC 0x62656570
#endif
#ifndef FUSE_CTL_SUPER_MAGIC
# define FUSE_CTL_SUPER_MAGIC 0x65735543
#endif
#ifndef TRACEFS_MAGIC
# define TRACEFS_MAGIC 0x74726163
#endif
#ifndef CGROUP2_SUPER_MAGIC
# define CGROUP2_SUPER_MAGIC 0x63677270
#endif
#ifndef BPF_FS_MAGIC
# define BPF_FS_MAGIC 0xcafe4a11
#endif
#ifndef NSFS_MAGIC
# define NSFS_MAGIC 0x6e736673
#endif
#ifndef EFIVARFS_MAGIC
# define EFIVARFS_MAGIC 0xde5e81e4
#endif

#if defined(__SIHD_LINUX__)
# include <mntent.h> // setmntent
#endif

namespace sihd::sys::fs
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::fs");

namespace
{

enum class CopyResult
{
    done,
    unsupported,
    stopped,
};

// copy_file_range/sendfile accept at most this many bytes per call
constexpr size_t range_chunk_max = 0x7ffff000;

bool do_stat(std::string_view path, struct stat *s)
{
    return ::stat(path.data(), s) == 0;
}

#if defined(SIHD_FS_HAS_STATX)

Timestamp statx_timestamp_to_ts(const struct statx_timestamp & ts)
{
    // statx seconds are 64-bit even on 32-bit platforms where time_t is long
    return Timestamp(timespec {(time_t)ts.tv_sec, (long)ts.tv_nsec});
}

#endif

bool write_full(int fd, const char *data, size_t size)
{
    while (size > 0)
    {
        ssize_t n = ::write(fd, data, size);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            SIHD_LOG(error, "fs: copy_file: write: {}", os::last_error_str());
            return false;
        }
        data += n;
        size -= (size_t)n;
    }
    return true;
}

template <typename CopyChunk>
CopyResult kernel_copy_loop(CopyChunk copy_chunk, size_t total, const std::function<bool(size_t, size_t)> & progress)
{
    size_t transferred = 0;
    while (transferred < total)
    {
        ssize_t n = copy_chunk(std::min(total - transferred, range_chunk_max));
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            // these leave the fd offsets usable for the next strategy
            if (errno == EINVAL || errno == ENOSYS || errno == EXDEV || errno == EOPNOTSUPP)
                return CopyResult::unsupported;
            SIHD_LOG(error, "fs: copy_file: kernel copy: {}", os::last_error_str());
            return CopyResult::stopped;
        }
        if (n == 0)
        {
            // the source shrank under fstat; do not report a short copy as done
            return CopyResult::stopped;
        }
        transferred += (size_t)n;
        if (progress && !progress(transferred, total))
            return CopyResult::stopped;
    }
    return CopyResult::done;
}

CopyResult
    read_write_copy_loop(int in_fd, int out_fd, size_t total, const std::function<bool(size_t, size_t)> & progress)
{
    std::vector<char> buf(256 * 1024);
    size_t transferred = 0;
    for (;;)
    {
        ssize_t n = ::read(in_fd, buf.data(), buf.size());
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            SIHD_LOG(error, "fs: copy_file: read: {}", os::last_error_str());
            return CopyResult::stopped;
        }
        if (n == 0)
            return CopyResult::done;
        if (!write_full(out_fd, buf.data(), (size_t)n))
            return CopyResult::stopped;
        transferred += (size_t)n;
        if (progress && !progress(transferred, total))
            return CopyResult::stopped;
    }
}

#if defined(__linux__)

CopyResult kernel_copy(int in_fd, int out_fd, size_t total, const std::function<bool(size_t, size_t)> & progress)
{
    CopyResult r = kernel_copy_loop(
        [&](size_t chunk) { return ::copy_file_range(in_fd, nullptr, out_fd, nullptr, chunk, 0); },
        total,
        progress);
    if (r != CopyResult::unsupported)
        return r;
    // copy_file_range wants a regular file on the same mount; sendfile covers more
    return kernel_copy_loop([&](size_t chunk) { return ::sendfile(out_fd, in_fd, nullptr, chunk); }, total, progress);
}

#endif

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

// default location, relative to the home directory
std::string home_subpath(std::string_view sub)
{
    const std::string home = home_path();
    return home.empty() ? "" : combine(home, sub);
}

// XDG base-dir spec: the variable wins only when it holds an absolute path
std::string xdg_path(const char *var, std::string_view default_under_home)
{
    const std::optional<std::string> from_env = env::get(var);
    if (from_env.has_value() && is_absolute(*from_env))
        return *from_env;
    return home_subpath(default_under_home);
}

// XDG user-dirs spec: 'XDG_...="$HOME/..."' lines of user-dirs.dirs
std::string user_dirs_path(std::string_view key)
{
    const std::string config_dir = xdg_path("XDG_CONFIG_HOME", ".config");
    if (config_dir.empty())
        return "";
    const std::optional<std::string> content = read_all(combine(config_dir, "user-dirs.dirs"));
    if (!content.has_value())
        return "";
    const std::string home = home_path();
    for (const std::string & line : str::split(content.value(), '\n'))
    {
        const auto [line_key, line_value] = str::split_pair(line, "=");
        if (str::trim(line_key) != key)
            continue;
        std::string dir(str::trim(line_value));
        if (dir.size() >= 2 && dir.front() == '"' && dir.back() == '"')
            dir = dir.substr(1, dir.size() - 2);
        if (str::starts_with(dir, "$HOME"))
        {
            if (home.empty())
                continue;
            dir.replace(0, 5, home);
        }
        if (!dir.empty() && is_absolute(dir))
            return dir;
    }
    return "";
}

} // namespace

std::string home_path()
{
    return env::get("HOME").value_or("");
}

std::string config_path()
{
    return xdg_path("XDG_CONFIG_HOME", ".config");
}

std::string data_path()
{
    return xdg_path("XDG_DATA_HOME", ".local/share");
}

std::string cache_path()
{
    return xdg_path("XDG_CACHE_HOME", ".cache");
}

std::string download_path()
{
    const std::string dir = user_dirs_path("XDG_DOWNLOAD_DIR");
    return dir.empty() ? home_subpath("Downloads") : dir;
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

std::optional<FileTimes> times(std::string_view path)
{
#if defined(SIHD_FS_HAS_STATX)
    struct statx stx;
    if (::statx(AT_FDCWD, path.data(), 0, STATX_BASIC_STATS | STATX_BTIME, &stx) != 0)
        return std::nullopt;
    FileTimes ret;
    if (stx.stx_mask & STATX_BTIME)
        ret.creation = statx_timestamp_to_ts(stx.stx_btime);
    if (stx.stx_mask & STATX_ATIME)
        ret.access = statx_timestamp_to_ts(stx.stx_atime);
    if (stx.stx_mask & STATX_MTIME)
        ret.write = statx_timestamp_to_ts(stx.stx_mtime);
    return ret;
#else
    struct stat s;
    if (!do_stat(path.data(), &s))
        return std::nullopt;
    FileTimes ret;
    ret.access = Timestamp(time::seconds(s.st_atime));
    ret.write = Timestamp(time::seconds(s.st_mtime));
    return ret;
#endif
}

Timestamp last_write(std::string_view path)
{
    return times(path).value_or(FileTimes {}).write;
}

std::optional<size_t> file_size(std::string_view path)
{
    struct stat s;
    return do_stat(path.data(), &s) ? s.st_size : std::optional<size_t> {};
}

// directories

std::string tmp_path()
{
    for (const char *var : {"TMPDIR", "TMP", "TEMP"})
    {
        const std::optional<std::string> path = env::get(var);
        if (path.has_value())
            return *path;
    }
    return "/tmp";
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

bool copy_file(std::string_view from, std::string_view to, const std::function<bool(size_t, size_t)> & progress)
{
    int in_fd = ::open(from.data(), O_RDONLY);
    if (in_fd < 0)
    {
        SIHD_LOG(error, "fs: copy_file: open '{}': {}", from, os::last_error_str());
        return false;
    }
    struct stat s;
    if (::fstat(in_fd, &s) != 0)
    {
        SIHD_LOG(error, "fs: copy_file: fstat '{}': {}", from, os::last_error_str());
        ::close(in_fd);
        return false;
    }
    const size_t total = s.st_size > 0 ? (size_t)s.st_size : 0;

    // opening the destination would truncate the source itself
    struct stat dst;
    if (::stat(to.data(), &dst) == 0 && dst.st_dev == s.st_dev && dst.st_ino == s.st_ino)
    {
        SIHD_LOG(error, "fs: copy_file: '{}' and '{}' are the same file", from, to);
        ::close(in_fd);
        return false;
    }

    int out_fd = ::open(to.data(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0)
    {
        SIHD_LOG(error, "fs: copy_file: open '{}': {}", to, os::last_error_str());
        ::close(in_fd);
        return false;
    }

    bool ret = false;
#if defined(__linux__)
    if (total > 0)
    {
        CopyResult r = kernel_copy(in_fd, out_fd, total, progress);
        if (r == CopyResult::unsupported)
            r = read_write_copy_loop(in_fd, out_fd, total, progress);
        ret = r == CopyResult::done;
    }
    else
#endif
    {
        // unknown size (empty file, procfs, pipes): read/write is the only strategy reaching EOF
        ret = read_write_copy_loop(in_fd, out_fd, total, progress) == CopyResult::done;
    }

    ::close(in_fd);
    if (ret)
    {
        // CopyFile2 preserves the source metadata; mirror it for symmetry
        ::fchmod(out_fd, s.st_mode & 07777);
#if defined(SIHD_FS_HAS_STATX)
        const struct timespec times[2] = {s.st_atim, s.st_mtim};
        if (::futimens(out_fd, times) != 0)
            ret = false;
#endif
    }
    if (::close(out_fd) != 0)
        ret = false;
    if (!ret)
        ::unlink(to.data());
    return ret;
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
        case RAMFS_MAGIC:
            return MountType::ram;
        case SQUASHFS_MAGIC:
            return MountType::readonly;
        // pseudo filesystems have no storage backing
        case PROC_SUPER_MAGIC:
        case SYSFS_MAGIC:
        case DEVTMPFS_MAGIC:
        case DEVPTS_SUPER_MAGIC:
        case HUGETLBFS_MAGIC:
        case CGROUP_SUPER_MAGIC:
        case CGROUP2_SUPER_MAGIC:
        case BPF_FS_MAGIC:
        case NSFS_MAGIC:
        case PIPEFS_MAGIC:
        case SOCKFS_MAGIC:
        case MQUEUE_MAGIC:
        case DEBUGFS_MAGIC:
        case TRACEFS_MAGIC:
        case CONFIGFS_MAGIC:
        case SECURITYFS_MAGIC:
        case SELINUX_MAGIC:
        case BINFMTFS_MAGIC:
        case PSTOREFS_MAGIC:
        case FUSE_CTL_SUPER_MAGIC:
        case EFIVARFS_MAGIC:
            return MountType::unknown;
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

std::optional<uint64_t> free_space(std::string_view path)
{
    struct statvfs vfs;
    if (::statvfs(path.data(), &vfs) != 0)
        return std::nullopt;
    return (uint64_t)vfs.f_bavail * vfs.f_frsize;
}

std::optional<uint64_t> total_space(std::string_view path)
{
    struct statvfs vfs;
    if (::statvfs(path.data(), &vfs) != 0)
        return std::nullopt;
    return (uint64_t)vfs.f_blocks * vfs.f_frsize;
}

#if defined(__SIHD_LINUX__)

std::vector<MountEntry> mounts()
{
    std::vector<MountEntry> ret;
    FILE *file = setmntent("/proc/mounts", "r");
    if (file == nullptr)
        return ret;
    mntent entry;
    char buf[4096];
    while (getmntent_r(file, &entry, buf, sizeof(buf)) != nullptr)
    {
        ret.push_back({entry.mnt_fsname, entry.mnt_dir, entry.mnt_type, mount_type(entry.mnt_dir)});
    }
    endmntent(file);
    return ret;
}

#else

std::vector<MountEntry> mounts()
{
    return {};
}

#endif

} // namespace sihd::sys::fs
