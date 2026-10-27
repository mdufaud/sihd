#ifndef __SIHD_CORE_TEST_HELPER_HPP__
#define __SIHD_CORE_TEST_HELPER_HPP__

#include <string>

#include <gtest/gtest.h>

#include <sihd/core/Device.hpp>

namespace test
{

sihd::core::Channel *channel_of(sihd::core::Device *device, const std::string & name);

template <typename T>
T first_value_of(sihd::core::Device *device, const std::string & name)
{
    sihd::core::Channel *channel = channel_of(device, name);
    EXPECT_NE(channel, nullptr) << name;
    return channel != nullptr ? channel->read<T>(0) : T {};
}

} // namespace test

#endif
