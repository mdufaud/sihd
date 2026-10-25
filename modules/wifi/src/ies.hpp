#ifndef __SIHD_WIFI_IES_HPP__
#define __SIHD_WIFI_IES_HPP__

#include <string>

#include <sihd/util/ArrayView.hpp>

namespace sihd::wifi::ies
{

// empty when the elements carry no ssid
std::string ssid(sihd::util::ArrUByteView information_elements);

} // namespace sihd::wifi::ies

#endif
