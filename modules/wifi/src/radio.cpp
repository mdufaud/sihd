#include <sihd/wifi/radio.hpp>

namespace sihd::wifi
{

std::string mode_str(Radio::Mode mode)
{
    switch (mode)
    {
        case Radio::Mode::station:
            return "station";
        case Radio::Mode::access_point:
            return "access point";
        case Radio::Mode::ad_hoc:
            return "ad-hoc";
        case Radio::Mode::mesh:
            return "mesh";
        case Radio::Mode::monitor:
            return "monitor";
        case Radio::Mode::unknown:
            break;
    }
    return "unknown";
}

} // namespace sihd::wifi
