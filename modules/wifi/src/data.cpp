#include <algorithm>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/data.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t llc_snap_size = 8;
constexpr size_t oui_offset = 3;
constexpr size_t ethertype_offset = 6;
constexpr uint8_t snap_sap = 0xaa;
constexpr uint8_t unnumbered_control = 0x03;

} // namespace

Result<Llc> parse_llc_snap(ArrUByteView body)
{
    if (body.size() < llc_snap_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "llc snap header needs {} bytes", llc_snap_size));
    if (body[0] != snap_sap || body[1] != snap_sap || body[2] != unnumbered_control)
        return std::unexpected(Error(ErrorCode::not_supported, "llc header is not snap"));

    Llc llc;
    std::copy_n(body.data() + oui_offset, llc.oui.size(), llc.oui.begin());
    llc.ethertype = endian::read_be<uint16_t>(body.data() + ethertype_offset, 2);
    llc.payload = ArrUByteView(body.data() + llc_snap_size, body.size() - llc_snap_size);
    return llc;
}

} // namespace sihd::wifi
