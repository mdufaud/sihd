#include <charconv>

#include <sihd/util/TokenPattern.hpp>

namespace sihd::util
{

namespace
{

constexpr size_t max_width = 4096;
constexpr std::string_view digits = "0123456789";
constexpr std::string_view name_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

bool is_align_char(char c)
{
    return c == '<' || c == '>' || c == '^';
}

size_t end_of_run(std::string_view view, std::string_view set, size_t from)
{
    const size_t end = view.find_first_not_of(set, from);
    return end == std::string_view::npos ? view.size() : end;
}

bool parse_classical(std::string_view spec, TokenPattern::Options & options)
{
    size_t i = 0;
    // std::format rule: a second align char makes the first one the fill
    if (i + 1 < spec.size() && is_align_char(spec[i + 1]))
    {
        options.fill = spec[i];
        options.align = spec[i + 1];
        i += 2;
    }
    else if (i < spec.size() && is_align_char(spec[i]))
    {
        options.align = spec[i];
        ++i;
    }
    const size_t width_end = end_of_run(spec, digits, i);
    if (width_end > i)
    {
        const std::from_chars_result res = std::from_chars(spec.data() + i, spec.data() + width_end, options.width);
        if (res.ec != std::errc())
            return false;
    }
    i = width_end;
    if (i < spec.size() && spec[i] == '.')
    {
        ++i;
        const size_t precision_end = end_of_run(spec, digits, i);
        if (precision_end == i)
            return false;
        const std::from_chars_result res = std::from_chars(spec.data() + i,
                                                           spec.data() + precision_end,
                                                           options.precision);
        if (res.ec != std::errc())
            return false;
        i = precision_end;
    }
    return i == spec.size();
}

std::expected<TokenPattern::Token, Error> parse_field(std::string_view view, size_t & index)
{
    TokenPattern::Token token;
    token.offset = index;
    ++index;
    const size_t name_begin = index;
    index = end_of_run(view, name_chars, index);
    token.name = view.substr(name_begin, index - name_begin);
    if (token.name.empty())
        return std::unexpected(Error(ErrorCode::invalid_argument, "empty field name at {}", token.offset));
    token.classical = true;
    if (index < view.size() && view[index] == ':')
    {
        ++index;
        const size_t spec_begin = index;
        index = view.find('}', index);
        if (index == std::string_view::npos)
            return std::unexpected(Error(ErrorCode::invalid_argument, "unterminated field at {}", token.offset));
        token.raw_spec = view.substr(spec_begin, index - spec_begin);
        token.classical = parse_classical(token.raw_spec, token.options);
        if (token.classical && token.options.width > max_width)
            return std::unexpected(Error(ErrorCode::invalid_argument, "width too large at {}", token.offset));
    }
    if (index == view.size() || view[index] != '}')
        return std::unexpected(Error(ErrorCode::invalid_argument, "unterminated field at {}", token.offset));
    ++index;
    return token;
}

} // namespace

std::expected<void, Error> TokenPattern::compile(std::string_view pattern)
{
    // scan on a private buffer: a refusal leaves the members untouched
    std::unique_ptr<std::string> owned = std::make_unique<std::string>(pattern);
    const std::string_view view(*owned);
    std::vector<Token> tokens;
    size_t index = 0;
    while (index < view.size())
    {
        const size_t brace = view.find_first_of("{}", index);
        const size_t end = brace == std::string_view::npos ? view.size() : brace;
        if (end != index)
        {
            Token token;
            token.text = view.substr(index, end - index);
            tokens.push_back(token);
            index = end;
        }
        if (brace == std::string_view::npos)
            break;
        if (brace + 1 < view.size() && view[brace + 1] == view[brace])
        {
            Token token;
            token.text = view.substr(brace, 1);
            tokens.push_back(token);
            index = brace + 2;
            continue;
        }
        if (view[brace] == '}')
            return std::unexpected(Error(ErrorCode::invalid_argument, "unmatched '}}' at {}", brace));
        std::expected<Token, Error> field = parse_field(view, index);
        if (field.has_value() == false)
            return std::unexpected(std::move(field).error());
        tokens.push_back(std::move(*field));
    }
    size_t reserve_hint = 0;
    for (const Token & token : tokens)
        reserve_hint += token.name.empty() ? token.text.size() : token.options.width;
    _pattern = std::move(owned);
    _tokens = std::move(tokens);
    _reserve_hint = reserve_hint;
    return {};
}

void TokenPattern::append(std::string & out, std::string_view value, const Options & options)
{
    std::string_view v = value;
    if (options.precision != 0 && options.precision < v.size())
        v = v.substr(0, options.precision);
    const size_t pad = options.width > v.size() ? options.width - v.size() : 0;
    if (options.align == '<')
    {
        out += v;
        out.append(pad, options.fill);
    }
    else if (options.align == '^')
    {
        const size_t left = pad / 2;
        out.append(left, options.fill);
        out += v;
        out.append(pad - left, options.fill);
    }
    else
    {
        out.append(pad, options.fill);
        out += v;
    }
}

} // namespace sihd::util
