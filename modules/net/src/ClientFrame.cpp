#include <cstring>

#include <sihd/net/ClientFrame.hpp>

namespace sihd::net
{

using namespace sihd::util;

ClientFrame::ClientFrame(uint64_t client_id, ArrCharView payload): _client_id(client_id), _payload(payload) {}

Result<ClientFrame> ClientFrame::decode(ArrByteView frame)
{
    if (frame.byte_size() < header_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "client frame shorter than its header"));
    uint64_t client_id = 0;
    std::memcpy(&client_id, frame.buf(), header_size);
    return ClientFrame(client_id, ArrCharView(frame.data() + header_size, frame.byte_size() - header_size));
}

ArrByte ClientFrame::encode() const
{
    ArrByte frame;
    frame.resize(header_size + _payload.byte_size());
    std::memcpy(frame.buf(), &_client_id, header_size);
    if (_payload.byte_size() > 0)
        std::memcpy(frame.data() + header_size, _payload.buf(), _payload.byte_size());
    return frame;
}

uint64_t ClientFrame::client_id() const
{
    return _client_id;
}

ArrCharView ClientFrame::payload() const
{
    return _payload;
}

} // namespace sihd::net
