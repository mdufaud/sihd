#ifndef __SIHD_CORE_CHANNELWAITER_HPP__
#define __SIHD_CORE_CHANNELWAITER_HPP__

#include <sihd/core/Channel.hpp>
#include <sihd/util/ObserverWaiter.hpp>

namespace sihd::core
{

using ChannelWaiter = sihd::util::ObserverWaiter<Channel>;

} // namespace sihd::core

#endif