#ifndef __SIHD_SYS_DEVICE_TREE_HPP__
#define __SIHD_SYS_DEVICE_TREE_HPP__

#include <sihd/core/Device.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::device_tree
{

// devices come from the factories linked in the binary; an error leaves what was loaded before it in place
sihd::util::Result<void> load(sihd::core::Device & parent, const sihd::json::Json & tree);

} // namespace sihd::sys::device_tree

#endif
