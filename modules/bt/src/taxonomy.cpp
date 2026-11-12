#include <algorithm>
#include <array>
#include <initializer_list>

#include <sihd/bt/taxonomy.hpp>

#include "uuid.hpp"

namespace sihd::bt::taxonomy
{

namespace
{

// bluetooth sig company identifiers
constexpr uint16_t apple_company = 0x004c;
constexpr uint16_t microsoft_company = 0x0006;

// member uuids the service data is keyed by
constexpr uint16_t eddystone_uuid = 0xfeaa;
constexpr uint16_t fast_pair_uuid = 0xfe2c;
constexpr uint16_t exposure_notification_uuid = 0xfd6f;

struct UuidKind
{
        uint16_t uuid;
        Kind kind;
};

constexpr auto uuid_kinds = std::to_array<UuidKind>({
    {0x1108, Kind::headphones},  // headset
    {0x110b, Kind::speaker},     // audio sink
    {0x1112, Kind::phone},       // headset audio gateway
    {0x1116, Kind::network},     // nap
    {0x1117, Kind::network},     // gn
    {0x1118, Kind::imaging},     // direct printing
    {0x111e, Kind::headphones},  // hands-free
    {0x111f, Kind::phone},       // ag hands-free
    {0x1124, Kind::hid},         // hid
    {0x1126, Kind::imaging},     // hcr print
    {0x112f, Kind::phone},       // phonebook access server
    {0x1131, Kind::headphones},  // headset - hs
    {0x1400, Kind::health},      // hdp
    {0x1808, Kind::health},      // glucose
    {0x1809, Kind::health},      // health thermometer
    {0x180d, Kind::health},      // heart rate
    {0x1810, Kind::health},      // blood pressure
    {0x1812, Kind::hid},         // human interface device
    {0x1814, Kind::sensor},      // running speed and cadence
    {0x1816, Kind::sensor},      // cycling speed and cadence
    {0x1818, Kind::sensor},      // cycling power
    {0x1819, Kind::sensor},      // location and navigation
    {0x181a, Kind::sensor},      // environmental sensing
    {0x181d, Kind::health},      // weight scale
    {0x181f, Kind::health},      // continuous glucose monitoring
    {0x1822, Kind::health},      // pulse oximeter
    {0x1827, Kind::network},     // mesh provisioning
    {0x1828, Kind::network},     // mesh proxy
    {0x1854, Kind::hearing_aid}, // hearing access
});

bool manufacturer_starts_with(const ScanResult & result, uint16_t company, std::initializer_list<uint8_t> prefix)
{
    const auto it = result.manufacturer_data.find(company);
    return it != result.manufacturer_data.end() && it->second.size() >= prefix.size()
           && std::equal(prefix.begin(), prefix.end(), it->second.begin());
}

Kind kind_from_beacon(Beacon beacon)
{
    switch (beacon)
    {
        case Beacon::ibeacon:
        case Beacon::eddystone:
            return Kind::beacon;
        case Beacon::find_my:
            return Kind::tag;
        case Beacon::exposure_notification:
            return Kind::phone;
        default:
            // any device announces its pairing
            return Kind::unknown;
    }
}

} // namespace

std::string_view kind_str(Kind kind)
{
    switch (kind)
    {
        case Kind::phone:
            return "phone";
        case Kind::computer:
            return "computer";
        case Kind::watch:
            return "watch";
        case Kind::wearable:
            return "wearable";
        case Kind::keyboard:
            return "keyboard";
        case Kind::mouse:
            return "mouse";
        case Kind::gamepad:
            return "gamepad";
        case Kind::hid:
            return "hid";
        case Kind::headphones:
            return "headphones";
        case Kind::speaker:
            return "speaker";
        case Kind::media:
            return "media";
        case Kind::hearing_aid:
            return "hearing_aid";
        case Kind::health:
            return "health";
        case Kind::sensor:
            return "sensor";
        case Kind::tag:
            return "tag";
        case Kind::beacon:
            return "beacon";
        case Kind::display:
            return "display";
        case Kind::imaging:
            return "imaging";
        case Kind::light:
            return "light";
        case Kind::appliance:
            return "appliance";
        case Kind::vehicle:
            return "vehicle";
        case Kind::network:
            return "network";
        case Kind::game_console:
            return "game_console";
        case Kind::toy:
            return "toy";
        default:
            return "unknown";
    }
}

std::string_view beacon_str(Beacon beacon)
{
    switch (beacon)
    {
        case Beacon::ibeacon:
            return "ibeacon";
        case Beacon::find_my:
            return "find_my";
        case Beacon::swift_pair:
            return "swift_pair";
        case Beacon::eddystone:
            return "eddystone";
        case Beacon::fast_pair:
            return "fast_pair";
        default:
            return "exposure_notification";
    }
}

Kind kind_from_uuid(std::string_view uuid)
{
    const std::optional<uint16_t> short_uuid = short_from_uuid(uuid);
    if (!short_uuid)
        return Kind::unknown;
    const auto it = std::ranges::find(uuid_kinds, *short_uuid, &UuidKind::uuid);
    return it == uuid_kinds.end() ? Kind::unknown : it->kind;
}

std::optional<Beacon> beacon(const ScanResult & result)
{
    // apple proximity beacon type, then its 21 bytes length
    if (manufacturer_starts_with(result, apple_company, {0x02, 0x15}))
        return Beacon::ibeacon;
    // apple offline finding type
    if (manufacturer_starts_with(result, apple_company, {0x12}))
        return Beacon::find_my;
    // microsoft beacon id of swift pair
    if (manufacturer_starts_with(result, microsoft_company, {0x03}))
        return Beacon::swift_pair;
    for (const auto & [uuid, data] : result.service_data)
    {
        switch (short_from_uuid(uuid).value_or(0))
        {
            case eddystone_uuid:
                return Beacon::eddystone;
            case fast_pair_uuid:
                return Beacon::fast_pair;
            case exposure_notification_uuid:
                return Beacon::exposure_notification;
            default:
                break;
        }
    }
    return std::nullopt;
}

Kind kind(const ScanResult & result)
{
    const Kind from_appearance = result.appearance ? kind_from_appearance(*result.appearance) : Kind::unknown;
    if (from_appearance != Kind::unknown)
        return from_appearance;
    const Kind from_class = result.class_of_device ? kind_from_class(*result.class_of_device) : Kind::unknown;
    if (from_class != Kind::unknown)
        return from_class;
    for (const std::string & uuid : result.uuids)
    {
        if (const Kind from_uuid = kind_from_uuid(uuid); from_uuid != Kind::unknown)
            return from_uuid;
    }
    for (const auto & [uuid, data] : result.service_data)
    {
        if (const Kind from_uuid = kind_from_uuid(uuid); from_uuid != Kind::unknown)
            return from_uuid;
    }
    const std::optional<Beacon> found = beacon(result);
    return found ? kind_from_beacon(*found) : Kind::unknown;
}

} // namespace sihd::bt::taxonomy
