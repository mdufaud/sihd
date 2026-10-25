#include <algorithm>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/management.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t beacon_fixed_size = 12;
constexpr size_t assoc_request_fixed_size = 4;
constexpr size_t reassoc_request_fixed_size = 10;
constexpr size_t assoc_response_fixed_size = 6;
constexpr size_t authentication_fixed_size = 6;
constexpr size_t reason_size = 2;
constexpr size_t action_fixed_size = 1;

// offsets in the body, layouts in docs/protocols/wifi-frames.md
constexpr size_t timestamp_size = 8;
constexpr size_t beacon_interval_offset = 8;
constexpr size_t beacon_capability_offset = 10;
constexpr size_t capability_offset = 0;
constexpr size_t listen_interval_offset = 2;
constexpr size_t current_ap_offset = 4;
constexpr size_t status_offset = 2;
constexpr size_t aid_offset = 4;
constexpr size_t algorithm_offset = 0;
constexpr size_t sequence_offset = 2;
constexpr size_t authentication_status_offset = 4;

constexpr uint16_t capability_ess = 0x0001;
constexpr uint16_t capability_ibss = 0x0002;
constexpr uint16_t capability_privacy = 0x0010;
constexpr uint16_t aid_mask = 0x3fff;

Error too_short(const char *what, size_t needed)
{
    return Error(ErrorCode::invalid_argument, "{} needs {} bytes", what, needed);
}

uint16_t u16_at(ArrUByteView body, size_t offset)
{
    return endian::read_le<uint16_t>(body.data() + offset, 2);
}

ArrUByteView after(ArrUByteView body, size_t offset)
{
    return ArrUByteView(body.data() + offset, body.size() - offset);
}

} // namespace

Result<Beacon> parse_beacon(ArrUByteView body)
{
    if (body.size() < beacon_fixed_size)
        return std::unexpected(too_short("beacon", beacon_fixed_size));

    Beacon beacon;
    beacon.timestamp_us = endian::read_le<uint64_t>(body.data(), timestamp_size);
    beacon.interval_tu = u16_at(body, beacon_interval_offset);
    beacon.capability = u16_at(body, beacon_capability_offset);
    beacon.ess = (beacon.capability & capability_ess) != 0;
    beacon.ibss = (beacon.capability & capability_ibss) != 0;
    beacon.privacy = (beacon.capability & capability_privacy) != 0;
    beacon.information_elements = after(body, beacon_fixed_size);
    return beacon;
}

Result<Beacon> parse_probe_response(ArrUByteView body)
{
    return parse_beacon(body);
}

Result<AssocRequest> parse_assoc_request(ArrUByteView body)
{
    if (body.size() < assoc_request_fixed_size)
        return std::unexpected(too_short("association request", assoc_request_fixed_size));

    AssocRequest request;
    request.capability = u16_at(body, capability_offset);
    request.listen_interval = u16_at(body, listen_interval_offset);
    request.information_elements = after(body, assoc_request_fixed_size);
    return request;
}

Result<ReassocRequest> parse_reassoc_request(ArrUByteView body)
{
    if (body.size() < reassoc_request_fixed_size)
        return std::unexpected(too_short("reassociation request", reassoc_request_fixed_size));

    ReassocRequest request;
    request.capability = u16_at(body, capability_offset);
    request.listen_interval = u16_at(body, listen_interval_offset);
    std::copy_n(body.data() + current_ap_offset, request.current_ap.size(), request.current_ap.begin());
    request.information_elements = after(body, reassoc_request_fixed_size);
    return request;
}

Result<AssocResponse> parse_assoc_response(ArrUByteView body)
{
    if (body.size() < assoc_response_fixed_size)
        return std::unexpected(too_short("association response", assoc_response_fixed_size));

    AssocResponse response;
    response.capability = u16_at(body, capability_offset);
    response.status = u16_at(body, status_offset);
    response.aid = static_cast<uint16_t>(u16_at(body, aid_offset) & aid_mask);
    response.information_elements = after(body, assoc_response_fixed_size);
    return response;
}

Result<Authentication> parse_authentication(ArrUByteView body)
{
    if (body.size() < authentication_fixed_size)
        return std::unexpected(too_short("authentication", authentication_fixed_size));

    Authentication authentication;
    authentication.algorithm = u16_at(body, algorithm_offset);
    authentication.sequence = u16_at(body, sequence_offset);
    authentication.status = u16_at(body, authentication_status_offset);
    authentication.information_elements = after(body, authentication_fixed_size);
    return authentication;
}

Result<uint16_t> parse_reason(ArrUByteView body)
{
    if (body.size() < reason_size)
        return std::unexpected(too_short("reason", reason_size));
    return u16_at(body, 0);
}

Result<Action> parse_action(ArrUByteView body)
{
    if (body.size() < action_fixed_size)
        return std::unexpected(too_short("action", action_fixed_size));

    Action action;
    action.category = body[0];
    action.body = after(body, action_fixed_size);
    return action;
}

} // namespace sihd::wifi
