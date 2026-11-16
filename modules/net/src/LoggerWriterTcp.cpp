#include <sihd/net/LoggerWriterTcp.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/endian.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(LoggerWriterTcp)

using namespace sihd::util;

LoggerWriterTcp::LoggerWriterTcp(const std::string & name, Node *parent): ALoggerWriter(name, parent, SOCK_STREAM) {}

LoggerWriterTcp::~LoggerWriterTcp() = default;

ArrByte LoggerWriterTcp::encode_frame(const LoggerRecord & record) const
{
    const uint32_t size = static_cast<uint32_t>(record.encoded_size());
    ArrByte frame;
    frame.resize(sizeof(size) + size);
    // the record size, big endian, before each record
    endian::write_be(size, frame.buf(), sizeof(size));
    record.encode_to(frame.buf() + sizeof(size));
    return frame;
}

} // namespace sihd::net
