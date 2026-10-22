#ifndef __SIHD_CORE_CHANNELMATCH_HPP__
#define __SIHD_CORE_CHANNELMATCH_HPP__

#include <string>
#include <string_view>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Value.hpp>

namespace sihd::core
{

class ChannelMatch
{
    public:
        enum Comparison
        {
            None,
            Equal,
            Superior,
            SuperiorEqual,
            Inferior,
            InferiorEqual,
            ByteAnd,
            ByteOr,
            ByteXor,
        };

        ChannelMatch() = default;
        ChannelMatch(Comparison comparison, sihd::util::Value value, size_t idx = 0);

        // "value" or "idx:value", the comparison is left unchanged
        sihd::util::Result<void> parse_trigger(std::string_view conf);
        // "cmp=equal;idx=0;value=false;invert=true"
        sihd::util::Result<void> parse(std::string_view conf);

        bool match(const Channel *channel) const;

        // compares an already read value, the channel is not read again
        bool match(const sihd::util::Value & in_value) const;

        // comparison set, index in range and float value only against a floating channel
        bool verify(const Channel *channel) const;

        static const char *comparison_str(Comparison comparison);
        static Comparison comparison_from_str(std::string_view str);

        static ChannelMatch equal(sihd::util::Value val);
        static ChannelMatch superior(sihd::util::Value val);
        static ChannelMatch superior_equal(sihd::util::Value val);
        static ChannelMatch inferior(sihd::util::Value val);
        static ChannelMatch inferior_equal(sihd::util::Value val);
        static ChannelMatch byte_and(sihd::util::Value val);
        static ChannelMatch byte_or(sihd::util::Value val);
        static ChannelMatch byte_xor(sihd::util::Value val);

        Comparison comparison = None;
        size_t idx = 0;
        sihd::util::Value value {0};
        bool invert = false;
};

} // namespace sihd::core

#endif
