#ifndef __SIHD_NET_LOGGERWRITERUDP_HPP__
#define __SIHD_NET_LOGGERWRITERUDP_HPP__

#include <sihd/net/ALoggerWriter.hpp>

namespace sihd::net
{

class LoggerWriterUdp: public ALoggerWriter
{
    public:
        LoggerWriterUdp(const std::string & name, sihd::util::Node *parent = nullptr);
        ~LoggerWriterUdp();

    protected:
        sihd::util::ArrByte encode_frame(const LoggerRecord & record) const override;
};

} // namespace sihd::net

#endif
