#include <sihd/usb/taxonomy.hpp>

namespace sihd::usb::taxonomy
{

namespace
{

// the device class 0x00 defers to the interfaces, 0xef marks an interface association
constexpr uint8_t per_interface_class = 0x00;
constexpr uint8_t miscellaneous_class = 0xef;

} // namespace

std::string_view kind_str(Kind kind)
{
    switch (kind)
    {
        case Kind::audio:
            return "audio";
        case Kind::video:
            return "video";
        case Kind::keyboard:
            return "keyboard";
        case Kind::mouse:
            return "mouse";
        case Kind::hid:
            return "hid";
        case Kind::storage:
            return "storage";
        case Kind::hub:
            return "hub";
        case Kind::printer:
            return "printer";
        case Kind::image:
            return "image";
        case Kind::serial:
            return "serial";
        case Kind::network:
            return "network";
        case Kind::bluetooth:
            return "bluetooth";
        case Kind::smart_card:
            return "smart_card";
        case Kind::healthcare:
            return "healthcare";
        case Kind::billboard:
            return "billboard";
        case Kind::firmware_update:
            return "firmware_update";
        case Kind::diagnostic:
            return "diagnostic";
        case Kind::vendor_specific:
            return "vendor_specific";
        default:
            return "unknown";
    }
}

Kind kind(const UsbDevice & device)
{
    if (device.device_class != per_interface_class && device.device_class != miscellaneous_class)
        return kind_from_class(device.device_class, device.device_subclass, device.device_protocol);
    Kind first = Kind::unknown;
    for (const UsbConfiguration & configuration : device.configurations)
    {
        for (const UsbInterface & usb_interface : configuration.interfaces)
        {
            for (const UsbAltSetting & setting : usb_interface.alt_settings)
            {
                const Kind found = kind_from_class(setting.interface_class,
                                                   setting.interface_subclass,
                                                   setting.interface_protocol);
                if (found == Kind::keyboard || found == Kind::mouse)
                    return found;
                if (first == Kind::unknown)
                    first = found;
            }
        }
    }
    return first;
}

} // namespace sihd::usb::taxonomy
