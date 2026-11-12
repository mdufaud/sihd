#ifndef __SIHD_PCAP_CAPTUREDPACKET_HPP__
#define __SIHD_PCAP_CAPTUREDPACKET_HPP__

#include <cstdint>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::pcap
{

// capture time in nanoseconds, length on the wire and datalink in host byte order, then the captured bytes
class CapturedPacket
{
    public:
        static constexpr size_t header_size = sizeof(int64_t) + sizeof(uint32_t) + sizeof(int32_t);

        CapturedPacket();
        CapturedPacket(sihd::util::Timestamp timestamp,
                       uint32_t original_length,
                       int datalink,
                       sihd::util::ArrUByteView data);
        explicit CapturedPacket(sihd::util::ArrUByteView packet);
        ~CapturedPacket() = default;

        // a failure keeps the packet; the data points into packet
        sihd::util::Result<void> decode(sihd::util::ArrUByteView packet);

        sihd::util::ArrUByte encode() const;

        sihd::util::Timestamp timestamp() const;
        uint32_t original_length() const;
        int datalink() const;
        sihd::util::ArrUByteView data() const;

    private:
        sihd::util::Timestamp _timestamp;
        uint32_t _original_length;
        int _datalink;
        sihd::util::ArrUByteView _data;
};

} // namespace sihd::pcap

#endif
