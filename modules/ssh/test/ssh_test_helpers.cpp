#include "ssh_test_helpers.hpp"

#include <libssh/sftp.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/endian.hpp>

using namespace sihd::util;

namespace test
{

SIHD_LOGGER;

namespace
{

template <typename T>
std::string big_endian(T value)
{
    std::string bytes(sizeof(T), '\0');
    endian::write_be(value, reinterpret_cast<uint8_t *>(bytes.data()), sizeof(T));
    return bytes;
}

} // namespace

bool connect_to_test_server(const SshServerHelper & server,
                            sihd::ssh::SshSession & session,
                            const char *user,
                            const char *password)
{
    // process_config=false: ignore ~/.ssh/config (proxy) for localhost tests
    if (SIHD_UNEXPECTED_LOG(
            session.fast_connect({.user = user, .host = "127.0.0.1", .port = server.port, .process_config = false})))
        return false;
    if (!session.connected())
        return false;
    auto auth = session.auth_password(password);
    return auth.success();
}

int SftpPacketClient::Packet::status() const
{
    return type == SSH_FXP_STATUS ? static_cast<int>(read_u32(body, 0)) : -1;
}

std::string SftpPacketClient::Packet::string_at(size_t offset) const
{
    if (body.size() < offset + 4)
        return {};
    return body.substr(offset + 4, read_u32(body, offset));
}

SftpPacketClient::SftpPacketClient(sihd::ssh::SshSession & session): _session(session) {}

bool SftpPacketClient::open()
{
    if (SIHD_UNEXPECTED_LOG(_session.make_channel_session(_channel)))
        return false;
    if (SIHD_UNEXPECTED_LOG(_channel.request_sftp()))
        return false;
    if (!this->send(SSH_FXP_INIT, u32(3)))
        return false;
    auto version = this->receive();
    return version.has_value() && version->type == SSH_FXP_VERSION;
}

std::optional<SftpPacketClient::Packet> SftpPacketClient::request(uint8_t type, const std::string & payload)
{
    const uint32_t id = _next_id++;
    if (!this->send(type, u32(id) + payload))
        return std::nullopt;
    auto packet = this->receive();
    if (!packet.has_value() || read_u32(packet->body, 0) != id)
        return std::nullopt;
    packet->body.erase(0, 4);
    return packet;
}

std::optional<SftpPacketClient::Packet> SftpPacketClient::request_path(uint8_t type, std::string_view path)
{
    return this->request(type, str(path));
}

std::string SftpPacketClient::open_file(std::string_view path, uint32_t pflags)
{
    auto reply = this->request(SSH_FXP_OPEN, str(path) + u32(pflags) + u32(0));
    if (!reply.has_value() || reply->type != SSH_FXP_HANDLE)
        return {};
    return reply->string_at(0);
}

std::string SftpPacketClient::u32(uint32_t value)
{
    return big_endian(value);
}

std::string SftpPacketClient::u64(uint64_t value)
{
    return big_endian(value);
}

std::string SftpPacketClient::str(std::string_view value)
{
    return u32(static_cast<uint32_t>(value.size())) + std::string(value);
}

uint32_t SftpPacketClient::read_u32(std::string_view data, size_t offset)
{
    if (data.size() < offset + 4)
        return UINT32_MAX;
    return endian::read_be<uint32_t>(reinterpret_cast<const uint8_t *>(data.data() + offset), sizeof(uint32_t));
}

bool SftpPacketClient::send(uint8_t type, const std::string & payload)
{
    const std::string packet = u32(static_cast<uint32_t>(payload.size() + 1)) + static_cast<char>(type) + payload;
    size_t sent = 0;
    while (sent < packet.size())
    {
        const int written = _channel.write({packet.data() + sent, packet.size() - sent});
        if (written <= 0)
            return false;
        sent += static_cast<size_t>(written);
    }
    return true;
}

std::optional<SftpPacketClient::Packet> SftpPacketClient::receive()
{
    std::string header;
    if (!this->read_exact(header, 5))
        return std::nullopt;
    Packet packet;
    packet.type = static_cast<uint8_t>(header[4]);
    const uint32_t length = read_u32(header, 0);
    if (length > 1 && !this->read_exact(packet.body, length - 1))
        return std::nullopt;
    return packet;
}

bool SftpPacketClient::read_exact(std::string & out, size_t size)
{
    out.clear();
    ArrChar chunk;
    while (out.size() < size)
    {
        chunk.reserve(size - out.size());
        if (_channel.read_timeout(chunk, 5000) <= 0)
            return false;
        out.append(chunk.data(), chunk.size());
    }
    return true;
}

} // namespace test
