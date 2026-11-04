#include <sihd/wifi/radio.hpp>

#include "wlanapi.hpp"

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

Radio::Mode mode_from_operation_mode(ULONG operation_mode)
{
    switch (operation_mode)
    {
        case DOT11_OPERATION_MODE_STATION:
        case DOT11_OPERATION_MODE_EXTENSIBLE_STATION:
        case DOT11_OPERATION_MODE_WFD_CLIENT:
            return Radio::Mode::station;
        case DOT11_OPERATION_MODE_AP:
        case DOT11_OPERATION_MODE_EXTENSIBLE_AP:
        case DOT11_OPERATION_MODE_WFD_GROUP_OWNER:
            return Radio::Mode::access_point;
        case DOT11_OPERATION_MODE_NETWORK_MONITOR:
            return Radio::Mode::monitor;
        default:
            return Radio::Mode::unknown;
    }
}

} // namespace

Result<Radio> radio(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto operation_mode = session.query_ulong(*guid, wlan_intf_opcode_current_operation_mode);
    SIHD_UNEXPECTED_RETURN(operation_mode);
    auto channel = session.query_ulong(*guid, wlan_intf_opcode_channel_number);
    SIHD_UNEXPECTED_RETURN(channel);

    Radio result;
    result.mode = mode_from_operation_mode(operation_mode->value_or(DOT11_OPERATION_MODE_UNKNOWN));
    result.channel = static_cast<uint8_t>(channel->value_or(0));
    return result;
}

} // namespace sihd::wifi
