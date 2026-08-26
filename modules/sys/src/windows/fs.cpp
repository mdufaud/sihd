#include <windows.h> // HANDLE / CreateFileA / CloseHandle / DeviceIoControl
#include <winioctl.h> // IOCTL_STORAGE_QUERY_PROPERTY / IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS

#include <direct.h> // _mkdir _rmdir
#include <fcntl.h>  // _O_WRONLY
#include <io.h>     // _access _open _close _chsize_s
#include <sys/stat.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <fmt/ranges.h>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::sys::fs
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::fs");

namespace
{

bool do_stat(std::string_view path, struct stat *s)
{
    return ::_stat(path.data(), reinterpret_cast<struct _stat *>(s)) == 0;
}

} // namespace

std::string home_path()
{
    return combine(getenv("HOMEDRIVE"), getenv("HOMEPATH"));
}

std::string executable_path()
{
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH) != 0)
        return path;
    return ".";
}

// stat

bool exists(std::string_view path)
{
    return _access(path.data(), 0) == 0;
}

bool is_readable(std::string_view path)
{
    return _access(path.data(), 04) == 0;
}

bool is_writable(std::string_view path)
{
    return _access(path.data(), 02) == 0;
}

bool is_executable(std::string_view path)
{
    return _access(path.data(), 04) == 0;
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
    try
    {
        return std::filesystem::temp_directory_path().string();
    }
    catch ([[maybe_unused]] const std::filesystem::filesystem_error & e)
    {
    }
    const char *tmp_path = getenv("Temp");
    return tmp_path != nullptr ? tmp_path : "C:\\Windows\\TEMP\\";
}

std::string make_tmp_directory(std::string_view prefix)
{
    (void)prefix;
    std::error_code ec;
    auto tmp_path = std::filesystem::temp_directory_path(ec);
    if (!ec)
    {
        char name[L_tmpnam];
        if (std::tmpnam(name))
        {
            std::string_view tmp_name = name;
            tmp_name.remove_prefix(1);
            tmp_path /= tmp_name;
            std::string path = tmp_path.string();
            if (make_directory(path))
                return path;
        }
    }
    return "";
}

bool make_directory(std::string_view path, unsigned int mode)
{
    if (is_dir(path))
        return true;
    if (path.empty())
        return false;
    (void)mode;
    return _mkdir(path.data()) == 0;
}

std::vector<std::string> children(std::string_view path)
{
    std::vector<std::string> ret;

    std::error_code ec;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::directory_iterator it {path, options, ec};
    std::filesystem::directory_iterator end;

    while (it != end)
    {
        // match the posix contract: basename only, trailing separator for directories
        std::string name = it->path().filename().string();
        if (it->is_directory(ec))
            name += sep_str();
        ret.push_back(name);
        it = it.increment(ec);
    }

    return ret;
}

std::vector<std::string> recursive_children(std::string_view path, uint32_t max_depth)
{
    std::vector<std::string> ret;

    std::error_code ec;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator it {path, options, ec};
    std::filesystem::recursive_directory_iterator end;

    while (it != end)
    {
        if (max_depth == 0 || (uint32_t)it.depth() < max_depth)
        {
            ret.push_back(it->path().string());
        }
        it = it.increment(ec);
    }

    return ret;
}

// files

bool truncate(std::string_view path, int64_t size)
{
    int fd = _open(path.data(), _O_WRONLY);
    if (fd < 0)
        return false;
    errno_t rc = _chsize_s(fd, size);
    _close(fd);
    return rc == 0;
}

std::string realpath(std::string_view path)
{
    // POSIX realpath requires every path component to exist; _fullpath is purely
    // lexical and succeeds for nonexistent paths -> guard to keep the same contract
    if (!fs::exists(path))
        return "";
    char resolved[PATH_MAX];
    if (_fullpath(resolved, path.data(), PATH_MAX) == nullptr)
        return "";
    return std::string(resolved);
}

bool chdir(std::string_view path)
{
    return ::_chdir(path.data()) == 0;
}

MountType mount_type([[maybe_unused]] std::string_view path)
{
    // UNC path (\\server\share) is always a network mount
    if (path.size() >= 2 && (path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/'))
        return MountType::network;

    // GetVolumePathNameA resolves a nonexistent path to its drive root (local);
    // match the POSIX statfs-fails behavior so unresolvable paths are unknown
    if (!exists(path))
        return MountType::unknown;

    char root[MAX_PATH];
    if (GetVolumePathNameA(std::string(path).c_str(), root, sizeof(root)) == 0)
        return MountType::unknown;

    switch (GetDriveTypeA(root))
    {
        case DRIVE_REMOTE:
            return MountType::network;
        case DRIVE_RAMDISK:
            return MountType::ram;
        case DRIVE_CDROM:
            return MountType::readonly;
        case DRIVE_FIXED:
        case DRIVE_REMOVABLE:
            return MountType::local;
        default:
            return MountType::unknown;
    }
}

StorageMedium storage_medium([[maybe_unused]] std::string_view path)
{
    // no rotational backing for these (or backing is a loop file)
    const MountType type = mount_type(path);
    if (type == MountType::network || type == MountType::ram || type == MountType::readonly)
        return StorageMedium::unknown;

    char root[MAX_PATH];
    if (GetVolumePathNameA(std::string(path).c_str(), root, sizeof(root)) == 0)
        return StorageMedium::unknown;

    // \\.\X: addressing the volume by its drive letter
    std::string volume_path = fmt::format("\\\\.\\{}:", root[0]);
    HANDLE volume = CreateFileA(volume_path.c_str(),
                                0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);
    if (volume == INVALID_HANDLE_VALUE)
        return StorageMedium::unknown;

    StorageMedium ret = StorageMedium::unknown;
    VOLUME_DISK_EXTENTS extents;
    DWORD bytes = 0;
    if (DeviceIoControl(volume,
                        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                        nullptr,
                        0,
                        &extents,
                        sizeof(extents),
                        &bytes,
                        nullptr)
        && extents.NumberOfDiskExtents > 0)
    {
        std::string disk_path = fmt::format("\\\\.\\PhysicalDrive{}", extents.Extents[0].DiskNumber);
        HANDLE disk = CreateFileA(disk_path.c_str(),
                                  0,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  0,
                                  nullptr);
        if (disk != INVALID_HANDLE_VALUE)
        {
            STORAGE_PROPERTY_QUERY query {};
            query.PropertyId = StorageDeviceSeekPenaltyProperty;
            query.QueryType = PropertyStandardQuery;
            DEVICE_SEEK_PENALTY_DESCRIPTOR desc {};
            if (DeviceIoControl(disk,
                                IOCTL_STORAGE_QUERY_PROPERTY,
                                &query,
                                sizeof(query),
                                &desc,
                                sizeof(desc),
                                &bytes,
                                nullptr))
            {
                ret = desc.IncursSeekPenalty ? StorageMedium::hdd : StorageMedium::ssd;
            }
            CloseHandle(disk);
        }
    }
    CloseHandle(volume);
    return ret;
}

} // namespace sihd::sys::fs
