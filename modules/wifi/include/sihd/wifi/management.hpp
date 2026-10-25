#ifndef __SIHD_WIFI_MANAGEMENT_HPP__
#define __SIHD_WIFI_MANAGEMENT_HPP__

#include <cstdint>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/frame.hpp>

namespace sihd::wifi
{

struct Beacon
{
        uint64_t timestamp_us = 0;
        // in time units of 1024 microseconds
        uint16_t interval_tu = 0;
        uint16_t capability = 0;
        bool ess = false;
        bool ibss = false;
        bool privacy = false;
        sihd::util::ArrUByteView information_elements;
};

struct AssocRequest
{
        uint16_t capability = 0;
        uint16_t listen_interval = 0;
        sihd::util::ArrUByteView information_elements;
};

struct ReassocRequest
{
        uint16_t capability = 0;
        uint16_t listen_interval = 0;
        MacAddress current_ap = {};
        sihd::util::ArrUByteView information_elements;
};

struct AssocResponse
{
        uint16_t capability = 0;
        uint16_t status = 0;
        uint16_t aid = 0;
        sihd::util::ArrUByteView information_elements;
};

struct Authentication
{
        uint16_t algorithm = 0;
        uint16_t sequence = 0;
        uint16_t status = 0;
        sihd::util::ArrUByteView information_elements;
};

struct Action
{
        uint8_t category = 0;
        sihd::util::ArrUByteView body;
};

// parsers take Frame::body, invalid_argument when shorter than the fixed fields (a probe request has none)
sihd::util::Result<Beacon> parse_beacon(sihd::util::ArrUByteView body);
sihd::util::Result<Beacon> parse_probe_response(sihd::util::ArrUByteView body);
sihd::util::Result<AssocRequest> parse_assoc_request(sihd::util::ArrUByteView body);
sihd::util::Result<ReassocRequest> parse_reassoc_request(sihd::util::ArrUByteView body);
sihd::util::Result<AssocResponse> parse_assoc_response(sihd::util::ArrUByteView body);
sihd::util::Result<Authentication> parse_authentication(sihd::util::ArrUByteView body);
// reason code of a deauthentication or a disassociation; check Frame::protected_frame first
sihd::util::Result<uint16_t> parse_reason(sihd::util::ArrUByteView body);
// same trap as parse_reason(): an action frame can be protected
sihd::util::Result<Action> parse_action(sihd::util::ArrUByteView body);

} // namespace sihd::wifi

#endif
