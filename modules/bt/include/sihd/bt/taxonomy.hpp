#ifndef __SIHD_BT_TAXONOMY_HPP__
#define __SIHD_BT_TAXONOMY_HPP__

#include <cstdint>
#include <optional>
#include <string_view>

#include <sihd/bt/Scanner.hpp>

namespace sihd::bt::taxonomy
{

enum class Kind
{
    phone,
    computer,
    watch,
    wearable,
    keyboard,
    mouse,
    gamepad,
    hid,
    headphones,
    speaker,
    media,
    hearing_aid,
    health,
    sensor,
    tag,
    beacon,
    display,
    imaging,
    light,
    appliance,
    vehicle,
    network,
    game_console,
    toy,
    unknown
};

enum class Beacon
{
    ibeacon,
    find_my,
    swift_pair,
    eddystone,
    fast_pair,
    exposure_notification
};

std::string_view kind_str(Kind kind);
std::string_view beacon_str(Beacon beacon);

// the gap appearance: category in bits 15 to 6, sub-category in bits 5 to 0
constexpr Kind kind_from_appearance(uint16_t appearance)
{
    const uint16_t category = appearance >> 6;
    const uint16_t subcategory = appearance & 0x3f;
    switch (category)
    {
        case 0x001: // phone
            return Kind::phone;
        case 0x002: // computer
            return Kind::computer;
        case 0x003: // watch
            return Kind::watch;
        case 0x007: // eye-glasses
            return Kind::wearable;
        case 0x008: // tag
        case 0x009: // keyring
            return Kind::tag;
        case 0x00f: // human interface device
            switch (subcategory)
            {
                case 0x01: // keyboard
                    return Kind::keyboard;
                case 0x02: // mouse
                    return Kind::mouse;
                case 0x03: // joystick
                case 0x04: // gamepad
                    return Kind::gamepad;
                default:
                    return Kind::hid;
            }
        case 0x006: // remote control
        case 0x00b: // barcode scanner
        case 0x013: // control device
            return Kind::hid;
        case 0x025: // wearable audio device
            return Kind::headphones;
        case 0x021: // audio sink
            return Kind::speaker;
        case 0x00a: // media player
        case 0x022: // audio source
        case 0x027: // av equipment
            return Kind::media;
        case 0x029: // hearing aid
            return Kind::hearing_aid;
        case 0x00c: // thermometer
        case 0x00d: // heart rate sensor
        case 0x00e: // blood pressure
        case 0x010: // glucose meter
        case 0x031: // pulse oximeter
        case 0x032: // weight scale
        case 0x034: // continuous glucose monitor
        case 0x035: // insulin pump
        case 0x036: // medication delivery
        case 0x037: // spirometer
            return Kind::health;
        case 0x011: // running walking sensor
        case 0x012: // cycling
        case 0x015: // sensor
        case 0x051: // outdoor sports activity
        case 0x052: // industrial measurement device
            return Kind::sensor;
        case 0x005: // display
        case 0x028: // display equipment
        case 0x02b: // signage
            return Kind::display;
        case 0x016: // light fixtures
        case 0x01f: // light source
            return Kind::light;
        case 0x004: // clock
        case 0x017: // fan
        case 0x018: // hvac
        case 0x019: // air conditioning
        case 0x01a: // humidifier
        case 0x01b: // heating
        case 0x01c: // access control
        case 0x01d: // motorized device
        case 0x01e: // power device
        case 0x020: // window covering
        case 0x024: // domestic appliance
        case 0x053: // industrial tools
        case 0x054: // cookware device
            return Kind::appliance;
        case 0x023: // motorized vehicle
        case 0x026: // aircraft
        case 0x033: // personal mobility device
            return Kind::vehicle;
        case 0x014: // network device
            return Kind::network;
        case 0x02a: // gaming
            return Kind::game_console;
        default:
            return Kind::unknown;
    }
}

namespace detail
{

constexpr Kind kind_from_peripheral(uint8_t minor)
{
    // bits 7 and 6 of the class of device
    const uint8_t pointing_keyboard = minor >> 4;
    // bits 5 to 2
    const uint8_t subminor = minor & 0x0f;
    if (pointing_keyboard == 0x1 || pointing_keyboard == 0x3) // keyboard, combo keyboard and pointing device
        return Kind::keyboard;
    if (pointing_keyboard == 0x2) // pointing device
        return Kind::mouse;
    if (subminor == 0x1 || subminor == 0x2) // joystick, gamepad
        return Kind::gamepad;
    return Kind::hid;
}

constexpr Kind kind_from_audio_video(uint8_t minor)
{
    switch (minor)
    {
        case 0x01: // wearable headset device
        case 0x06: // headphones
            return Kind::headphones;
        case 0x02: // hands-free device
        case 0x05: // loudspeaker
        case 0x0a: // hifi audio device
            return Kind::speaker;
        case 0x08: // car audio
            return Kind::vehicle;
        case 0x0e: // video monitor
        case 0x0f: // video display and loudspeaker
            return Kind::display;
        case 0x12: // gaming/toy
            return Kind::game_console;
        case 0x13: // hearing aid
            return Kind::hearing_aid;
        case 0x14: // glasses
            return Kind::wearable;
        default:
            return Kind::media;
    }
}

} // namespace detail

// the class of device: major class in bits 12 to 8, minor class in bits 7 to 2
constexpr Kind kind_from_class(uint32_t class_of_device)
{
    const uint8_t major = (class_of_device >> 8) & 0x1f;
    const uint8_t minor = (class_of_device >> 2) & 0x3f;
    switch (major)
    {
        case 0x01: // computer
            return Kind::computer;
        case 0x02: // phone
            return Kind::phone;
        case 0x03: // lan/network access point
            return Kind::network;
        case 0x04: // audio/video
            return detail::kind_from_audio_video(minor);
        case 0x05: // peripheral
            return detail::kind_from_peripheral(minor);
        case 0x06: // imaging: bit 4 marks a display
            return (minor & 0x04) != 0 ? Kind::display : Kind::imaging;
        case 0x07: // wearable: minor 1 is a wristwatch
            return minor == 0x01 ? Kind::watch : Kind::wearable;
        case 0x08: // toy: minor 4 is a controller, 5 a game
            return minor == 0x04 ? Kind::gamepad : minor == 0x05 ? Kind::game_console : Kind::toy;
        case 0x09: // health: minor 15 is a personal mobility device
            return minor == 0x0f ? Kind::vehicle : Kind::health;
        default:
            return Kind::unknown;
    }
}

// a profile or service uuid, 16 bits in the 128 bits base form as ScanResult::uuids holds it
Kind kind_from_uuid(std::string_view uuid);

// recognizes the advertisement by its prefix, without decoding it
std::optional<Beacon> beacon(const ScanResult & result);

// tries the appearance, the class of device, the uuids then a known beacon
Kind kind(const ScanResult & result);

} // namespace sihd::bt::taxonomy

#endif
