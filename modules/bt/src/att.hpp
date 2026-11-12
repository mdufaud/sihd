#ifndef __SIHD_BT_ATT_HPP__
#define __SIHD_BT_ATT_HPP__

#include <string_view>

#include <sihd/util/Error.hpp>

namespace sihd::bt::att
{

// the error of a failed ATT request, its code following the Bluetooth core specification part F 3.4.1.1
sihd::util::Error error_of(int status, std::string_view what);

} // namespace sihd::bt::att

#endif
