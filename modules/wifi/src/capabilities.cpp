#include <array>
#include <string_view>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/capabilities.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t ht_info_offset = 0;
constexpr size_t ht_ampdu_offset = 2;
constexpr size_t ht_mcs_set_offset = 3;
constexpr size_t ht_mcs_set_size = 16;
constexpr size_t ht_min_size = ht_mcs_set_offset + ht_mcs_set_size;
constexpr size_t ht_rx_mask_streams = 4;
constexpr size_t ht_rx_highest_offset = 13;

constexpr uint16_t ht_ldpc = 0x0001;
constexpr uint16_t ht_width_40mhz = 0x0002;
constexpr uint16_t ht_smps_mask = 0x000c;
constexpr unsigned ht_smps_shift = 2;
constexpr uint16_t ht_greenfield = 0x0010;
constexpr uint16_t ht_short_gi_20mhz = 0x0020;
constexpr uint16_t ht_short_gi_40mhz = 0x0040;
constexpr uint16_t ht_tx_stbc = 0x0080;
constexpr uint16_t ht_rx_stbc_mask = 0x0300;
constexpr unsigned ht_rx_stbc_shift = 8;
constexpr uint16_t ht_max_amsdu = 0x0800;
constexpr uint16_t ht_dsss_cck_40mhz = 0x1000;
constexpr uint16_t ht_forty_mhz_intolerant = 0x4000;
constexpr uint8_t ht_ampdu_exponent_mask = 0x03;

constexpr uint8_t ht_ampdu_spacing_mask = 0x1c;
constexpr unsigned ht_ampdu_spacing_shift = 2;
constexpr uint16_t ht_rx_highest_mask = 0x03ff;

constexpr size_t vht_size = 12;
constexpr size_t vht_rx_map_offset = 4;
constexpr size_t vht_tx_map_offset = 8;
constexpr size_t vht_streams_max = 8;
constexpr uint16_t vht_stream_unsupported = 3;

constexpr uint32_t vht_mpdu_mask = 0x00000003;
constexpr uint32_t vht_width_mask = 0x0000000c;
constexpr unsigned vht_width_shift = 2;
constexpr uint32_t vht_rx_ldpc = 0x00000010;
constexpr uint32_t vht_short_gi_80mhz = 0x00000020;
constexpr uint32_t vht_short_gi_160mhz = 0x00000040;
constexpr uint32_t vht_tx_stbc = 0x00000080;
constexpr uint32_t vht_rx_stbc_mask = 0x00000700;
constexpr unsigned vht_rx_stbc_shift = 8;
constexpr uint32_t vht_su_beamformer = 0x00000800;
constexpr uint32_t vht_su_beamformee = 0x00001000;
constexpr uint32_t vht_mu_beamformer = 0x00080000;
constexpr uint32_t vht_mu_beamformee = 0x00100000;
constexpr uint32_t vht_ampdu_exponent_mask = 0x03800000;

constexpr unsigned vht_ampdu_exponent_shift = 23;

// the reserved value 3 gives 0
constexpr std::array<uint16_t, 4> vht_mpdu_lengths = {3895, 7991, 11454, 0};

constexpr size_t country_header_size = 3;
constexpr size_t country_code_size = 2;
constexpr size_t country_triplet_size = 3;
// a first byte from 201 starts an operating extension triplet, not a channel range
constexpr uint8_t country_last_channel_number = 200;

constexpr size_t tim_size = 3;
constexpr size_t tim_count_offset = 0;
constexpr size_t tim_period_offset = 1;
constexpr size_t tim_bitmap_control_offset = 2;
constexpr uint8_t tim_multicast_bit = 0x01;

constexpr uint8_t extension_he_capabilities = 35;
constexpr uint8_t extension_eht_capabilities = 108;

Error too_short(std::string_view what, size_t needed)
{
    return Error(ErrorCode::invalid_argument, "{} needs {} bytes", what, needed);
}

template <typename T>
Result<std::optional<T>> decode_element(const std::vector<ies::Element> & elements,
                                        ies::Id id,
                                        size_t min_size,
                                        std::string_view name,
                                        T (*decoder)(ArrUByteView))
{
    const std::optional<ies::Element> element = ies::find(elements, id);
    if (element.has_value() == false)
        return std::nullopt;
    if (element->data.size() < min_size)
        return std::unexpected(too_short(name, min_size));
    return decoder(element->data);
}

uint8_t map_streams(uint16_t map)
{
    uint8_t streams = 0;
    for (size_t stream = 0; stream < vht_streams_max; ++stream)
    {
        if (((map >> (stream * 2)) & vht_stream_unsupported) != vht_stream_unsupported)
            streams = static_cast<uint8_t>(stream + 1);
    }
    return streams;
}

} // namespace

namespace ies
{

namespace
{

HtCapabilities decode_ht(ArrUByteView body)
{
    const uint8_t *data = body.data();
    HtCapabilities ht;
    ht.info = endian::read_le<uint16_t>(data + ht_info_offset, 2);
    ht.ldpc = (ht.info & ht_ldpc) != 0;
    ht.width_40mhz = (ht.info & ht_width_40mhz) != 0;
    ht.spatial_multiplexing_power_save = static_cast<uint8_t>((ht.info & ht_smps_mask) >> ht_smps_shift);
    ht.greenfield = (ht.info & ht_greenfield) != 0;
    ht.short_gi_20mhz = (ht.info & ht_short_gi_20mhz) != 0;
    ht.short_gi_40mhz = (ht.info & ht_short_gi_40mhz) != 0;

    ht.tx_stbc = (ht.info & ht_tx_stbc) != 0;
    ht.rx_stbc_streams = static_cast<uint8_t>((ht.info & ht_rx_stbc_mask) >> ht_rx_stbc_shift);
    ht.max_amsdu_7935 = (ht.info & ht_max_amsdu) != 0;
    ht.dsss_cck_40mhz = (ht.info & ht_dsss_cck_40mhz) != 0;
    ht.forty_mhz_intolerant = (ht.info & ht_forty_mhz_intolerant) != 0;

    ht.ampdu_length_exponent = static_cast<uint8_t>(data[ht_ampdu_offset] & ht_ampdu_exponent_mask);
    ht.ampdu_min_spacing = static_cast<uint8_t>((data[ht_ampdu_offset] & ht_ampdu_spacing_mask)
                                                >> ht_ampdu_spacing_shift);
    for (size_t stream = 0; stream < ht_rx_mask_streams; ++stream)
    {
        if (data[ht_mcs_set_offset + stream] != 0)
            ht.rx_spatial_streams = static_cast<uint8_t>(stream + 1);
    }
    ht.rx_highest_rate_mbps = static_cast<uint16_t>(endian::read_le<uint16_t>(data + ht_rx_highest_offset, 2)
                                                    & ht_rx_highest_mask);
    return ht;
}

VhtCapabilities decode_vht(ArrUByteView body)
{
    const uint8_t *data = body.data();
    VhtCapabilities vht;
    vht.info = endian::read_le<uint32_t>(data, 4);
    vht.max_mpdu_length = vht_mpdu_lengths[vht.info & vht_mpdu_mask];
    const uint32_t width_set = (vht.info & vht_width_mask) >> vht_width_shift;
    vht.width_160mhz = width_set == 1 || width_set == 2;
    vht.width_80plus80mhz = width_set == 2;
    vht.rx_ldpc = (vht.info & vht_rx_ldpc) != 0;
    vht.short_gi_80mhz = (vht.info & vht_short_gi_80mhz) != 0;
    vht.short_gi_160mhz = (vht.info & vht_short_gi_160mhz) != 0;

    vht.tx_stbc = (vht.info & vht_tx_stbc) != 0;
    vht.rx_stbc_streams = static_cast<uint8_t>((vht.info & vht_rx_stbc_mask) >> vht_rx_stbc_shift);
    vht.su_beamformer = (vht.info & vht_su_beamformer) != 0;
    vht.su_beamformee = (vht.info & vht_su_beamformee) != 0;

    vht.mu_beamformer = (vht.info & vht_mu_beamformer) != 0;
    vht.mu_beamformee = (vht.info & vht_mu_beamformee) != 0;
    vht.ampdu_length_exponent = static_cast<uint8_t>((vht.info & vht_ampdu_exponent_mask) >> vht_ampdu_exponent_shift);
    vht.rx_mcs_map = endian::read_le<uint16_t>(data + vht_rx_map_offset, 2);
    vht.tx_mcs_map = endian::read_le<uint16_t>(data + vht_tx_map_offset, 2);
    vht.rx_spatial_streams = map_streams(vht.rx_mcs_map);
    vht.tx_spatial_streams = map_streams(vht.tx_mcs_map);
    return vht;
}

Country decode_country(ArrUByteView body)
{
    Country country;
    country.code = std::string(body.begin(), body.begin() + country_code_size);
    country.environment = static_cast<char>(body[country_code_size]);
    for (size_t offset = country_header_size; offset + country_triplet_size <= body.size();
         offset += country_triplet_size)
    {
        if (body[offset] > country_last_channel_number)
            continue;
        country.channels.push_back(
            {.first = body[offset], .count = body[offset + 1], .max_power_dbm = static_cast<int8_t>(body[offset + 2])});
    }
    return country;
}

Tim decode_tim(ArrUByteView body)
{
    Tim tim;
    tim.dtim_count = body[tim_count_offset];
    tim.dtim_period = body[tim_period_offset];
    tim.multicast_buffered = (body[tim_bitmap_control_offset] & tim_multicast_bit) != 0;
    return tim;
}

} // namespace

Result<std::optional<HtCapabilities>> ht_capabilities(const std::vector<Element> & elements)
{
    return decode_element(elements, Id::ht_capabilities, ht_min_size, "ht capabilities", &decode_ht);
}

Result<std::optional<VhtCapabilities>> vht_capabilities(const std::vector<Element> & elements)
{
    return decode_element(elements, Id::vht_capabilities, vht_size, "vht capabilities", &decode_vht);
}

Result<std::optional<Country>> country(const std::vector<Element> & elements)
{
    return decode_element(elements, Id::country, country_header_size, "country", &decode_country);
}

Result<std::optional<Tim>> tim(const std::vector<Element> & elements)
{
    return decode_element(elements, Id::tim, tim_size, "tim", &decode_tim);
}

Standards standards(const std::vector<Element> & elements)
{
    Standards result;
    result.ht = find(elements, Id::ht_capabilities).has_value();
    result.vht = find(elements, Id::vht_capabilities).has_value();
    for (const Element & element : elements)
    {
        if (element.id != Id::extension || element.data.size() < 1)
            continue;
        result.he = result.he || element.data[0] == extension_he_capabilities;
        result.eht = result.eht || element.data[0] == extension_eht_capabilities;
    }
    return result;
}

} // namespace ies

} // namespace sihd::wifi
