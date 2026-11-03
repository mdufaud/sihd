#ifndef __SIHD_SYS_WINDOWS_INTERNAL_COM_HPP__
#define __SIHD_SYS_WINDOWS_INTERNAL_COM_HPP__

#include <optional>

#include <sihd/sys/windows/ComInit.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::internal
{

sihd::util::Result<void> init_com(std::optional<windows_::ComInit> & com,
                                  windows_::ComInit::Threading threading = windows_::ComInit::Threading::apartment);

} // namespace sihd::sys::internal

#endif
