#include "drive_type.hpp"

using sihd::sys::windows_::volume::MediaType;

namespace sihd::sys::internal
{

MediaType media_type_from_native(UINT drive_type)
{
    switch (drive_type)
    {
        case DRIVE_REMOVABLE:
            return MediaType::removable;
        case DRIVE_FIXED:
            return MediaType::fixed;
        case DRIVE_REMOTE:
            return MediaType::remote;
        case DRIVE_CDROM:
            return MediaType::cdrom;
        case DRIVE_RAMDISK:
            return MediaType::ramdisk;
        default:
            return MediaType::unknown;
    }
}

} // namespace sihd::sys::internal
