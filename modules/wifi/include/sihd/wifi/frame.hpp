#ifndef __SIHD_WIFI_FRAME_HPP__
#define __SIHD_WIFI_FRAME_HPP__

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::wifi
{

using MacAddress = std::array<uint8_t, 6>;

using Oui = std::array<uint8_t, 3>;

struct Frame
{
        enum class Type : uint8_t
        {
            management = 0,
            control = 1,
            data = 2,
            extension = 3
        };

        enum class ManagementSubtype : uint8_t
        {
            assoc_request = 0,
            assoc_response = 1,
            reassoc_request = 2,
            reassoc_response = 3,
            probe_request = 4,
            probe_response = 5,
            timing_advertisement = 6,
            beacon = 8,
            atim = 9,
            disassociation = 10,
            authentication = 11,
            deauthentication = 12,
            action = 13,
            action_no_ack = 14
        };

        enum class ControlSubtype : uint8_t
        {
            trigger = 2,
            tack = 3,
            beamforming_report_poll = 4,
            vht_ndp_announcement = 5,
            control_frame_extension = 6,
            control_wrapper = 7,
            block_ack_request = 8,
            block_ack = 9,
            ps_poll = 10,
            rts = 11,
            cts = 12,
            ack = 13,
            cf_end = 14,
            cf_end_ack = 15
        };

        Type type = Type::management;
        // raw 4 bits: a ManagementSubtype or a ControlSubtype by `type`
        uint8_t subtype = 0;

        bool to_ds = false;
        bool from_ds = false;
        bool more_fragments = false;
        bool retry = false;
        bool power_management = false;
        bool more_data = false;
        bool protected_frame = false;
        bool order = false;

        uint16_t duration = 0;

        // the raw addresses; addresses() says which role each one plays
        MacAddress addr1 = {};
        std::optional<MacAddress> addr2;
        std::optional<MacAddress> addr3;
        std::optional<MacAddress> addr4;

        // absent in control frames
        std::optional<uint16_t> sequence_number;
        std::optional<uint8_t> fragment_number;
        // absent in control frames and in frames without a QoS control field
        std::optional<uint8_t> qos_tid;
        // the body is a series of subframes: parse_llc_snap() on it is wrong
        bool amsdu = false;
        // the header ends with 4 bytes of HT Control, not decoded
        bool ht_control = false;

        size_t header_size = 0;
        // view into the caller's buffer
        sihd::util::ArrUByteView body;
};

// a role the frame has none for is empty
struct Addresses
{
        MacAddress ra = {};
        std::optional<MacAddress> ta;
        std::optional<MacAddress> bssid;
        std::optional<MacAddress> sa;
        std::optional<MacAddress> da;
};

// invalid_argument when the frame is truncated or not protocol version 0, not_supported for extension frames
sihd::util::Result<Frame> parse_frame(sihd::util::ArrUByteView bytes);

// empty when `type` is another one or the value is reserved
std::optional<Frame::ManagementSubtype> management_subtype(const Frame & frame);
std::optional<Frame::ControlSubtype> control_subtype(const Frame & frame);

Addresses addresses(const Frame & frame);

// "AA:BB:CC:DD:EE:FF", either case; invalid_argument otherwise
sihd::util::Result<MacAddress> parse_mac(std::string_view text);

bool is_group(const MacAddress & mac);
bool is_locally_administered(const MacAddress & mac);

} // namespace sihd::wifi

#endif
