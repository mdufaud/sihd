#ifndef __SIHD_SYS_WINDOWS_VOLUME_HPP__
#define __SIHD_SYS_WINDOWS_VOLUME_HPP__

#include <cstdint>
#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# error "sihd/sys/windows headers are windows only"
#endif

namespace sihd::sys::windows_::volume
{

enum class MediaType
{
    unknown,
    removable,
    fixed,
    remote,
    cdrom,
    ramdisk,
};

struct Info
{
        std::string root;
        std::string label;
        std::string file_system;
        MediaType media_type = MediaType::unknown;
        uint32_t serial = 0;
};

sihd::util::Result<Info> query(const std::string & path);
sihd::util::Result<void> eject(const std::string & path);

} // namespace sihd::sys::windows_::volume

#endif
