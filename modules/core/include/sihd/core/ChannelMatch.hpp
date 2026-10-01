#ifndef __SIHD_CORE_CHANNELMATCH_HPP__
#define __SIHD_CORE_CHANNELMATCH_HPP__

#include <expected>
#include <string>
#include <string_view>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Value.hpp>

namespace sihd::core
{

// checks a channel value at an index against a comparison
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
        std::expected<void, sihd::util::Error> parse_trigger(std::string_view conf);
        // "cmp=equal;idx=0;value=false;invert=true"
        std::expected<void, sihd::util::Error> parse(std::string_view conf);

        bool match(const Channel *channel) const;

        // compares an already read value, the channel is not read again
        bool match(const sihd::util::Value & in_value) const;

        // comparison set, index in range and float value only against a floating channel
        bool verify(const Channel *channel) const;

        static const char *comparison_str(Comparison comparison);
        static Comparison comparison_from_str(std::string_view str);

        template <typename T>
        static ChannelMatch equal(T val)
        {
            return {Equal, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch superior(T val)
        {
            return {Superior, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch superior_equal(T val)
        {
            return {SuperiorEqual, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch inferior(T val)
        {
            return {Inferior, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch inferior_equal(T val)
        {
            return {InferiorEqual, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch byte_and(T val)
        {
            return {ByteAnd, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch byte_or(T val)
        {
            return {ByteOr, sihd::util::Value(val)};
        }

        template <typename T>
        static ChannelMatch byte_xor(T val)
        {
            return {ByteXor, sihd::util::Value(val)};
        }

        Comparison comparison = None;
        size_t idx = 0;
        sihd::util::Value value {0};
        bool invert = false;
};

} // namespace sihd::core

#endif
