#ifndef __SIHD_NET_LOGGERRECORD_HPP__
#define __SIHD_NET_LOGGERRECORD_HPP__

#include <cstdint>
#include <string_view>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::net
{

class LoggerRecord
{
    public:
        static constexpr uint8_t version = 1;

        LoggerRecord();
        // the record views info.source, info.thread_name and message: it must not outlive them
        LoggerRecord(const sihd::util::LogInfo & info, std::string_view message);
        explicit LoggerRecord(sihd::util::ArrByteView record);
        ~LoggerRecord() = default;

        // a failure keeps the record; the decoded texts view record
        sihd::util::Result<void> decode(sihd::util::ArrByteView record);
        sihd::util::ArrByte encode() const;
        size_t encoded_size() const;
        // data holds encoded_size() bytes
        void encode_to(uint8_t *data) const;

        sihd::util::LogInfo log_info() const;

        sihd::util::LogLevel level() const;
        sihd::util::Timestamp timestamp() const;
        std::string_view source() const;
        std::string_view thread_name() const;
        std::string_view message() const;

    private:
        sihd::util::LogLevel _level;
        sihd::util::Timestamp _timestamp;
        std::string_view _source;
        std::string_view _thread_name;
        std::string_view _message;
};

} // namespace sihd::net

#endif
