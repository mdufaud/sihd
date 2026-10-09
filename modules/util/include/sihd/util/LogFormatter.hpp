#ifndef __SIHD_UTIL_LOGFORMATTER_HPP__
#define __SIHD_UTIL_LOGFORMATTER_HPP__

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Error.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/TokenPattern.hpp>

namespace sihd::util
{

// formats a LogInfo through a {name} pattern: {msg} {source} {level} {thread} {tid} {epoch}
// {time:strftime} {utc:strftime} - the line ending lives in the pattern like any literal
class LogFormatter
{
    public:
        static constexpr std::string_view default_pattern = "{epoch}\t[{thread}]\t{level:<9} {source}\t{msg}\n";

        LogFormatter();

        std::expected<void, Error> set_pattern(std::string_view pattern);
        std::string format(const LogInfo & info, std::string_view msg) const;

        const std::string & pattern() const;

    private:
        struct Token
        {
                enum class Kind
                {
                    literal,
                    msg,
                    source,
                    level,
                    thread_name,
                    thread_id,
                    epoch,
                    time_local,
                    time_utc,
                };

                Kind kind = Kind::literal;
                // a view into the template's owned pattern
                std::string_view literal;
                TokenPattern::Options options;
                // a time field's strftime spec: a std::string, strftime reads a C string
                std::string time_format;
        };

        static std::optional<Token::Kind> _resolve_kind(std::string_view name);

        TokenPattern _template;
        std::vector<Token> _tokens;
};

} // namespace sihd::util

#endif
