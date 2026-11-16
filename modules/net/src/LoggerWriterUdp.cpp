#include <sihd/net/LoggerWriterUdp.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(LoggerWriterUdp)

using namespace sihd::util;

LoggerWriterUdp::LoggerWriterUdp(const std::string & name, Node *parent): ALoggerWriter(name, parent, SOCK_DGRAM) {}

LoggerWriterUdp::~LoggerWriterUdp() = default;

ArrByte LoggerWriterUdp::encode_frame(const LoggerRecord & record) const
{
    return record.encode();
}

} // namespace sihd::net
