#include <algorithm>

#include <sihd/wifi/dissect.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t padding_boundary = 4;

void skip_padding(Frame & frame)
{
    const size_t padding = (padding_boundary - frame.header_size % padding_boundary) % padding_boundary;
    const size_t skipped = std::min(padding, frame.body.size());
    frame.body = ArrUByteView(frame.body.data() + skipped, frame.body.size() - skipped);
}

} // namespace

Result<Packet> dissect_radiotap(ArrUByteView bytes)
{
    Result<Radiotap> radiotap = parse_radiotap(bytes);
    SIHD_UNEXPECTED_RETURN(radiotap);

    Result<Frame> frame = parse_frame(radiotap->payload);
    SIHD_UNEXPECTED_RETURN(frame);
    if (radiotap->data_padding)
        skip_padding(*frame);

    Packet packet;
    packet.radiotap = std::move(*radiotap);
    packet.frame = std::move(*frame);
    return packet;
}

Result<Packet> dissect_80211(ArrUByteView bytes)
{
    Result<Frame> frame = parse_frame(bytes);
    SIHD_UNEXPECTED_RETURN(frame);

    Packet packet;
    packet.frame = std::move(*frame);
    return packet;
}

} // namespace sihd::wifi
