#ifndef __SIHD_WIFI_TAXONOMY_HPP__
#define __SIHD_WIFI_TAXONOMY_HPP__

#include <string>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::wifi::taxonomy
{

// one half of a signature, from the elements of a single frame; a damaged element ends the list
std::string element_signature(sihd::util::ArrUByteView information_elements);

// the whole `wifi4|probe:...|assoc:...` line; invalid_argument when one half yields nothing
sihd::util::Result<std::string> signature(sihd::util::ArrUByteView probe_request_elements,
                                          sihd::util::ArrUByteView assoc_request_elements);

} // namespace sihd::wifi::taxonomy

#endif
