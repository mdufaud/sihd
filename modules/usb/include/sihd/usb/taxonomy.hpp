#ifndef __SIHD_USB_TAXONOMY_HPP__
#define __SIHD_USB_TAXONOMY_HPP__

#include <cstdint>
#include <string_view>

#include <sihd/usb/UsbDevice.hpp>

namespace sihd::usb::taxonomy
{

enum class Kind
{
    audio,
    video,
    keyboard,
    mouse,
    hid,
    storage,
    hub,
    printer,
    image,
    serial,
    network,
    bluetooth,
    smart_card,
    healthcare,
    billboard,
    firmware_update,
    diagnostic,
    vendor_specific,
    unknown
};

std::string_view kind_str(Kind kind);

namespace detail
{

constexpr Kind kind_from_communications(uint8_t subclass, uint8_t protocol)
{
    switch (subclass)
    {
        case 0x02: // abstract control model, protocol 0xff the microsoft rndis
            return protocol == 0xff ? Kind::network : Kind::serial;
        case 0x06: // ethernet networking control model
        case 0x07: // atm networking control model
        case 0x0c: // ethernet emulation model
        case 0x0d: // network control model
        case 0x0e: // mobile broadband interface model
            return Kind::network;
        default:
            return Kind::unknown;
    }
}

} // namespace detail

// the base class, subclass and protocol the usb-if defines, of a device or an interface descriptor
constexpr Kind kind_from_class(uint8_t class_code, uint8_t subclass, uint8_t protocol)
{
    switch (class_code)
    {
        case 0x01: // audio
            return Kind::audio;
        case 0x02: // communications and cdc control
            return detail::kind_from_communications(subclass, protocol);
        case 0x03: // hid: the boot interface subclass 0x01 gives protocol 0x01 keyboard, 0x02 mouse
            if (subclass == 0x01 && protocol == 0x01)
                return Kind::keyboard;
            if (subclass == 0x01 && protocol == 0x02)
                return Kind::mouse;
            return Kind::hid;
        case 0x06: // still imaging
            return Kind::image;
        case 0x07: // printer
            return Kind::printer;
        case 0x08: // mass storage
            return Kind::storage;
        case 0x09: // hub
            return Kind::hub;
        case 0x0b: // smart card
            return Kind::smart_card;
        case 0x0e: // video
            return Kind::video;
        case 0x0f: // personal healthcare
            return Kind::healthcare;
        case 0x10: // audio/video devices: subclass 0x03 the audio streaming interface
            return subclass == 0x03 ? Kind::audio : Kind::video;
        case 0x11: // billboard
            return Kind::billboard;
        case 0xdc: // diagnostic
            return Kind::diagnostic;
        case 0xe0: // wireless controller, subclass 0x01: protocol 0x01 bluetooth, 0x03 rndis, 0x04 bluetooth amp
            if (subclass == 0x01 && (protocol == 0x01 || protocol == 0x04))
                return Kind::bluetooth;
            return subclass == 0x01 && protocol == 0x03 ? Kind::network : Kind::unknown;
        case 0xef: // miscellaneous: subclass 0x04 protocol 0x01 rndis over ethernet
            return subclass == 0x04 && protocol == 0x01 ? Kind::network : Kind::unknown;
        case 0xfe: // application specific: subclass 0x01 device firmware upgrade
            return subclass == 0x01 ? Kind::firmware_update : Kind::unknown;
        case 0xff: // vendor specific
            return Kind::vendor_specific;
        default:
            return Kind::unknown;
    }
}

// the device class, else the first known interface of its configurations, a keyboard or a mouse first
Kind kind(const UsbDevice & device);

} // namespace sihd::usb::taxonomy

#endif
