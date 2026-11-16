#ifndef __SIHD_NET_LOGGERWRITERTCP_HPP__
#define __SIHD_NET_LOGGERWRITERTCP_HPP__

#include <sihd/net/ALoggerWriter.hpp>

namespace sihd::net
{

class LoggerWriterTcp: public ALoggerWriter
{
    public:
        LoggerWriterTcp(const std::string & name, sihd::util::Node *parent = nullptr);
        ~LoggerWriterTcp();

    protected:
        sihd::util::ArrByte encode_frame(const LoggerRecord & record) const override;
};

} // namespace sihd::net

#endif
