#include "test_helper.hpp"

namespace test
{

sihd::core::Channel *channel_of(sihd::core::Device *device, const std::string & name)
{
    return device->get_channel(name).value_or(nullptr);
}

} // namespace test
