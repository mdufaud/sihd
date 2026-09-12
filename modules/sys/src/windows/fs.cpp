#include <direct.h> // _mkdir _rmdir
#include <fcntl.h>  // _O_WRONLY
#include <io.h>     // _access _open _close _chsize_s
#include <shlobj.h> // SHGetKnownFolderPath / FOLDERID_*
#include <sys/stat.h>
#include <windows.h>  // HANDLE / CreateFileA / CloseHandle / DeviceIoControl
#include <winioctl.h> // IOCTL_STORAGE_QUERY_PROPERTY / IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS

#include <cstdio>
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

namespace sihd::sys::fs
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::fs");

namespace
{

struct CopyProgressCtx
{
        const std::function<bool(size_t, size_t)> *progress;
};

bool do_stat(std::string_view path, struct stat *s)
{
    return ::_stat(path.data(), reinterpret_cast<struct _stat *>(s)) == 0;
}

// FILETIME counts 100ns intervals since 1601-01-01; 0 means the field is not tracked
Timestamp filetime_to_ts(int64_t filetime)
{
    if (filetime == 0)
        return Timestamp(0);
    return Timestamp((filetime - 116444736000000000LL) * 100);
}

COPYFILE2_MESSAGE_ACTION CALLBACK copy_progress_cb(const COPYFILE2_MESSAGE *message, PVOID context)
{
    auto *ctx = (CopyProgressCtx *)context;
    if (message->Type == COPYFILE2_CALLBACK_CHUNK_FINISHED && ctx->progress && *ctx->progress)
    {
        const auto & chunk = message->Info.ChunkFinished;
        const size_t transferred = (size_t)chunk.uliTotalBytesTransferred.QuadPart;
        const size_t total = (size_t)chunk.uliTotalFileSize.QuadPart;
        if (!(*ctx->progress)(transferred, total))
            return COPYFILE2_PROGRESS_CANCEL; // CopyFile2 then removes the partial destination
    }
    return COPYFILE2_PROGRESS_CONTINUE;
}

MountType drive_type_to_mount(UINT type)
{
    switch (type)
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

// SHGetKnownFolderPath with KF_FLAG_DONT_VERIFY: the path is returned even when it doesn't exist
std::string known_folder(REFKNOWNFOLDERID folder_id)
{
    PWSTR wide = nullptr;
    if (::SHGetKnownFolderPath(folder_id, KF_FLAG_DONT_VERIFY, nullptr, &wide) != S_OK)
        return "";
    std::string path;
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size > 1)
    {
        path.resize(size - 1);
        ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, path.data(), size, nullptr, nullptr);
    }
    ::CoTaskMemFree(wide);
    return path;
}

std::string env_str(const char *name)
{
    return env::get(name).value_or("");
}

// roaming: settings and user data follow the user across machines
std::string roaming_folder()
{
    std::string path = known_folder(FOLDERID_RoamingAppData);
    return path.empty() ? env_str("APPDATA") : path;
}

} // namespace

std::string home_path()
{
    const std::optional<std::string> drive = env::get("HOMEDRIVE");
    const std::optional<std::string> path = env::get("HOMEPATH");
    if (!drive.has_value() || !path.has_value())
        return "";
    return combine(*drive, *path);
}

std::string config_path()
{
    return roaming_folder();
}

std::string data_path()
{
    return roaming_folder();
}

std::string cache_path()
{
    std::string path = known_folder(FOLDERID_LocalAppData);
    return path.empty() ? env_str("LOCALAPPDATA") : path;
}

std::string download_path()
{
    std::string path = known_folder(FOLDERID_Downloads);
    if (path.empty())
    {
        const std::optional<std::string> profile = env::get("USERPROFILE");
        if (profile.has_value())
            path = combine(*profile, "Downloads");
    }
    return path;
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

std::optional<FileTimes> times(std::string_view path)
{
    HANDLE handle = CreateFileA(path.data(),
                                // query access: metadata does not need read rights
                                0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr,
                                OPEN_EXISTING,
                                // backup semantics lets directories open too
                                FILE_FLAG_BACKUP_SEMANTICS,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return std::nullopt;
    FILE_BASIC_INFO info;
    const BOOL ok = GetFileInformationByHandleEx(handle, FileBasicInfo, &info, sizeof(info));
    CloseHandle(handle);
    if (!ok)
        return std::nullopt;
    FileTimes ret;
    ret.creation = filetime_to_ts(info.CreationTime.QuadPart);
    ret.access = filetime_to_ts(info.LastAccessTime.QuadPart);
    ret.write = filetime_to_ts(info.LastWriteTime.QuadPart);
    return ret;
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
    try
    {
        return std::filesystem::temp_directory_path().string();
    }
    catch ([[maybe_unused]] const std::filesystem::filesystem_error & e)
    {
    }
    const std::optional<std::string> tmp_path = env::get("Temp");
    return tmp_path.value_or("C:\\Windows\\TEMP\\");
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

bool copy_file(std::string_view from, std::string_view to, const std::function<bool(size_t, size_t)> & progress)
{
    const std::wstring wfrom = str::to_wstr(from);
    const std::wstring wto = str::to_wstr(to);
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0602
    CopyProgressCtx ctx {progress ? &progress : nullptr};

    COPYFILE2_EXTENDED_PARAMETERS params {};
    params.dwSize = sizeof(params);
    params.pProgressRoutine = &copy_progress_cb;
    params.pvCallbackContext = &ctx;

    // a failing CopyFile2 that never started the copy leaves the destination untouched:
    // only remove a destination we made or a cancel left partial
    const bool dest_existed = ::GetFileAttributesW(wto.c_str()) != INVALID_FILE_ATTRIBUTES;
    const HRESULT hr = ::CopyFile2(wfrom.c_str(), wto.c_str(), &params);
    if (SUCCEEDED(hr))
        return true;
    if (hr == HRESULT_FROM_WIN32(ERROR_REQUEST_ABORTED) || !dest_existed)
        ::DeleteFileW(wto.c_str());
    if (hr != HRESULT_FROM_WIN32(ERROR_REQUEST_ABORTED))
        SIHD_LOG(error, "fs: copy_file: CopyFile2: 0x{:08x}", (unsigned long)hr);
    return false;
#else
    // SDK without the CopyFile2 declarations (Win7-era): no progress support
    (void)progress;
    return ::CopyFileW(wfrom.c_str(), wto.c_str(), FALSE) != 0;
#endif
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

    return drive_type_to_mount(GetDriveTypeA(root));
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

std::optional<uint64_t> free_space(std::string_view path)
{
    ULARGE_INTEGER free_bytes;
    if (!GetDiskFreeSpaceExA(std::string(path).c_str(), &free_bytes, nullptr, nullptr))
        return std::nullopt;
    return free_bytes.QuadPart;
}

std::optional<uint64_t> total_space(std::string_view path)
{
    ULARGE_INTEGER total_bytes;
    if (!GetDiskFreeSpaceExA(std::string(path).c_str(), nullptr, &total_bytes, nullptr))
        return std::nullopt;
    return total_bytes.QuadPart;
}

std::vector<MountEntry> mounts()
{
    std::vector<MountEntry> ret;
    const DWORD drives = GetLogicalDrives();
    for (char letter = 'A'; letter <= 'Z'; ++letter)
    {
        if (!(drives & (1u << (letter - 'A'))))
            continue;
        const std::string root = fmt::format("{}:\\", letter);
        const UINT type = GetDriveTypeA(root.c_str());
        if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
            continue;
        char fs_name[MAX_PATH + 1] = {};
        GetVolumeInformationA(root.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fs_name, sizeof(fs_name));
        ret.push_back({root, root, fs_name, drive_type_to_mount(type)});
    }
    return ret;
}

} // namespace sihd::sys::fs
