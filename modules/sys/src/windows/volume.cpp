// clang-format off
#include <windows.h>
#include <winioctl.h>
// clang-format on

#include <array>

#include <sihd/sys/windows/volume.hpp>
#include <sihd/util/Defer.hpp>

#include "internal/drive_type.hpp"
#include "internal/win_error.hpp"

using namespace sihd::util;
using sihd::sys::internal::media_type_from_native;
using sihd::sys::internal::Win32Error;

namespace sihd::sys::windows_::volume
{

namespace
{

using PathBuffer = std::array<char, MAX_PATH + 1>;

Result<PathBuffer> volume_root(const std::string & path)
{
    PathBuffer root {};
    if (GetVolumePathNameA(path.c_str(), root.data(), static_cast<DWORD>(root.size())) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not find the volume of '{}': {}", path, error));
    }
    return root;
}

Result<void> volume_ioctl(HANDLE volume, DWORD code, void *input, DWORD input_size, const char *operation)
{
    DWORD returned = 0;
    if (DeviceIoControl(volume, code, input, input_size, nullptr, 0, &returned, nullptr) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not {} volume: {}", operation, error));
    }
    return {};
}

} // namespace

Result<Info> query(const std::string & path)
{
    auto root = volume_root(path);
    SIHD_UNEXPECTED_RETURN(root);

    PathBuffer label {};
    PathBuffer file_system {};
    DWORD serial = 0;
    if (GetVolumeInformationA(root->data(),
                              label.data(),
                              static_cast<DWORD>(label.size()),
                              &serial,
                              nullptr,
                              nullptr,
                              file_system.data(),
                              static_cast<DWORD>(file_system.size()))
        == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not query volume '{}': {}", root->data(), error));
    }

    return Info {
        .root = root->data(),
        .label = label.data(),
        .file_system = file_system.data(),
        .media_type = media_type_from_native(GetDriveTypeA(root->data())),
        .serial = serial,
    };
}

Result<void> eject(const std::string & path)
{
    auto root = volume_root(path);
    SIHD_UNEXPECTED_RETURN(root);

    PathBuffer name {};
    if (GetVolumeNameForVolumeMountPointA(root->data(), name.data(), static_cast<DWORD>(name.size())) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not find the volume name of '{}': {}", root->data(), error));
    }
    std::string device = name.data();
    // a trailing backslash opens the root directory instead of the volume
    device.pop_back();

    HANDLE volume = CreateFileA(device.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);
    if (volume == INVALID_HANDLE_VALUE)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not open volume '{}': {}", device, error));
    }
    Defer close_volume([volume] { CloseHandle(volume); });

    auto locked = volume_ioctl(volume, FSCTL_LOCK_VOLUME, nullptr, 0, "lock");
    SIHD_UNEXPECTED_RETURN(locked);
    auto dismounted = volume_ioctl(volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, "dismount");
    SIHD_UNEXPECTED_RETURN(dismounted);
    PREVENT_MEDIA_REMOVAL removal {.PreventMediaRemoval = FALSE};
    auto allowed = volume_ioctl(volume, IOCTL_STORAGE_MEDIA_REMOVAL, &removal, sizeof(removal), "allow removal of");
    SIHD_UNEXPECTED_RETURN(allowed);
    return volume_ioctl(volume, IOCTL_STORAGE_EJECT_MEDIA, nullptr, 0, "eject");
}

} // namespace sihd::sys::windows_::volume
