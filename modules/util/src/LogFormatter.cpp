#include <charconv>
#include <cstring>

#include <sihd/util/LogFormatter.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

std::optional<LogFormatter::Token::Kind> LogFormatter::_resolve_kind(std::string_view name)
{
    using Kind = Token::Kind;
    if (name == "msg")
        return Kind::msg;
    if (name == "source")
        return Kind::source;
    if (name == "level")
        return Kind::level;
    if (name == "thread")
        return Kind::thread_name;
    if (name == "tid")
        return Kind::thread_id;
    if (name == "epoch")
        return Kind::epoch;
    if (name == "time")
        return Kind::time_local;
    if (name == "utc")
        return Kind::time_utc;
    return std::nullopt;
}

LogFormatter::LogFormatter()
{
    (void)this->set_pattern(default_pattern);
}

std::expected<void, Error> LogFormatter::set_pattern(std::string_view pattern)
{
    TokenPattern compiled;
    if (auto res = compiled.compile(pattern); res.has_value() == false)
        return res;
    std::vector<Token> tokens;
    tokens.reserve(compiled.tokens().size());
    for (const TokenPattern::Token & token : compiled.tokens())
    {
        Token resolved;
        if (token.name.empty())
        {
            resolved.literal = token.text;
        }
        else
        {
            const std::optional<Token::Kind> kind = _resolve_kind(token.name);
            if (kind.has_value() == false)
                return std::unexpected(
                    Error(ErrorCode::invalid_argument, "unknown field '{}' at {}", token.name, token.offset));
            const bool time_field = *kind == Token::Kind::time_local || *kind == Token::Kind::time_utc;
            const bool spec_ok = token.raw_spec.empty()
                                 || (time_field ? token.raw_spec.find('%') != std::string_view::npos : token.classical);
            if (spec_ok == false)
                return std::unexpected(Error(ErrorCode::invalid_argument,
                                             "invalid spec '{}' for '{}' at {}",
                                             token.raw_spec,
                                             token.name,
                                             token.offset));
            if (time_field)
                resolved.time_format = token.raw_spec.empty() ? std::string(Timestamp::default_format)
                                                              : std::string(token.raw_spec);
            resolved.kind = *kind;
            resolved.options = token.options;
        }
        tokens.push_back(resolved);
    }
    _template = std::move(compiled);
    _tokens = std::move(tokens);
    return {};
}

std::string LogFormatter::format(const LogInfo & info, std::string_view msg) const
{
    std::string out;
    out.reserve(msg.size() + _template.reserve_hint() + 32);
    for (const Token & token : _tokens)
    {
        switch (token.kind)
        {
            case Token::Kind::literal:
                out += token.literal;
                break;
            case Token::Kind::msg:
                TokenPattern::append(out, msg, token.options);
                break;
            case Token::Kind::source:
                TokenPattern::append(out, info.source, token.options);
                break;
            case Token::Kind::level:
                TokenPattern::append(out, info.strlevel, token.options);
                break;
            case Token::Kind::thread_name:
                TokenPattern::append(out, info.thread_name, token.options);
                break;
            case Token::Kind::thread_id:
                TokenPattern::append(out, info.thread_id_str, token.options);
                break;
            case Token::Kind::epoch:
            {
                // to_chars, not fmt: an fmt call here cost hundreds of ns
                char buf[40];
                char *end = std::to_chars(buf, buf + sizeof(buf), info.timespec.tv_sec).ptr;
                *end++ = '.';
                char nsec_digits[16];
                const char *
                    nsec_end = std::to_chars(nsec_digits, nsec_digits + sizeof(nsec_digits), info.timespec.tv_nsec).ptr;
                const size_t digits = nsec_end - nsec_digits;
                for (size_t zeros = 9 - digits; zeros > 0; --zeros)
                    *end++ = '0';
                std::memcpy(end, nsec_digits, digits);
                TokenPattern::append(out, std::string_view(buf, end - buf + digits), token.options);
                break;
            }
            case Token::Kind::time_local:
            case Token::Kind::time_utc:
            {
                const std::string formatted = token.kind == Token::Kind::time_local
                                                  ? info.timestamp().local_format(token.time_format)
                                                  : info.timestamp().format(token.time_format);
                TokenPattern::append(out, formatted, token.options);
                break;
            }
        }
    }
    return out;
}

const std::string & LogFormatter::pattern() const
{
    return _template.pattern();
}

} // namespace sihd::util
