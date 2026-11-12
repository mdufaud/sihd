#ifndef __SIHD_NET_CLIENTFRAME_HPP__
#define __SIHD_NET_CLIENTFRAME_HPP__

#include <cstdint>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

// encoded as the client id in host byte order followed by the payload
class ClientFrame
{
    public:
        static constexpr size_t header_size = sizeof(uint64_t);

        ClientFrame();
        ClientFrame(uint64_t client_id, sihd::util::ArrCharView payload);
        explicit ClientFrame(sihd::util::ArrByteView frame);
        ~ClientFrame() = default;

        // a failure keeps the frame; the payload points into frame
        sihd::util::Result<void> decode(sihd::util::ArrByteView frame);

        sihd::util::ArrByte encode() const;

        uint64_t client_id() const;
        sihd::util::ArrCharView payload() const;

    private:
        uint64_t _client_id;
        sihd::util::ArrCharView _payload;
};

} // namespace sihd::net

#endif
