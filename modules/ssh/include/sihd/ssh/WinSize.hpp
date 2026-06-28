#ifndef __SIHD_SSH_WINSIZE_HPP__
#define __SIHD_SSH_WINSIZE_HPP__

#include <cstdint>

namespace sihd::ssh
{

struct WinSize
{
        uint16_t ws_row = 0;
        uint16_t ws_col = 0;
        uint16_t ws_xpixel = 0;
        uint16_t ws_ypixel = 0;
};

} // namespace sihd::ssh

#endif
