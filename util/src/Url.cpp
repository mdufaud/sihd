#include <sihd/util/Url.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

namespace
{

bool is_unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'
           || c == '.' || c == '~';
}

constexpr char hex_chars[] = "0123456789ABCDEF";

} // namespace

Url::Url(std::string_view url)
{
    std::string_view remainder = url;

    // 1. Extract scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":"
    auto colon = remainder.find(':');
    if (colon != std::string_view::npos && colon > 0)
    {
        auto potential = remainder.substr(0, colon);
        bool valid = (potential[0] >= 'A' && potential[0] <= 'Z') || (potential[0] >= 'a' && potential[0] <= 'z');
        for (size_t i = 1; valid && i < potential.size(); ++i)
        {
            char c = potential[i];
            valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-'
                    || c == '.';
        }
        if (valid)
        {
            scheme = std::string(potential);
            remainder = remainder.substr(colon + 1);
        }
    }

    // 2. Authority (optional) — "//" authority
    if (remainder.size() >= 2 && remainder[0] == '/' && remainder[1] == '/')
    {
        has_authority = true;
        remainder = remainder.substr(2);

        auto auth_end = remainder.find_first_of("/?#");
        std::string_view authority = remainder.substr(0, auth_end);
        remainder = (auth_end != std::string_view::npos) ? remainder.substr(auth_end) : std::string_view {};

        // userinfo: up to last '@'
        auto at = authority.rfind('@');
        if (at != std::string_view::npos)
        {
            userinfo = std::string(authority.substr(0, at));
            authority = authority.substr(at + 1);
        }

        // host + port: handle IPv6 [addr]:port
        if (!authority.empty() && authority[0] == '[')
        {
            auto bracket = authority.find(']');
            if (bracket != std::string_view::npos)
            {
                host = std::string(authority.substr(1, bracket - 1));
                authority = authority.substr(bracket + 1);
                if (!authority.empty() && authority[0] == ':')
                {
                    if (const auto port_val = sihd::util::str::convert_from_string<long>(authority.substr(1)))
                        port = static_cast<int>(*port_val);
                }
            }
        }
        else
        {
            auto port_colon = authority.rfind(':');
            if (port_colon != std::string_view::npos)
            {
                if (const auto port_val = sihd::util::str::convert_from_string<long>(authority.substr(port_colon + 1)))
                {
                    host = std::string(authority.substr(0, port_colon));
                    port = static_cast<int>(*port_val);
                }
                else
                    host = std::string(authority);
            }
            else
                host = std::string(authority);
        }
    }

    // 3. path, query, fragment
    if (!remainder.empty())
    {
        auto frag_pos = remainder.find('#');
        if (frag_pos != std::string_view::npos)
        {
            fragment = std::string(remainder.substr(frag_pos + 1));
            remainder = remainder.substr(0, frag_pos);
        }

        auto query_pos = remainder.find('?');
        if (query_pos != std::string_view::npos)
        {
            std::string_view query_str = remainder.substr(query_pos + 1);
            path = std::string(remainder.substr(0, query_pos));
            for (const auto & param : sihd::util::str::split(query_str, "&"))
            {
                auto [key, value] = sihd::util::str::split_pair(param, "=");
                if (!key.empty())
                    query_params[Url::str_decode(key)] = Url::str_decode(value);
            }
        }
        else
            path = std::string(remainder);
    }

    // default path for authority-based URLs
    if (has_authority && path.empty())
        path = "/";
}

Url Url::decode(std::string_view url)
{
    return Url(url);
}

std::string Url::str_encode(std::string_view str)
{
    std::string result;
    result.reserve(str.size() * 3);
    for (unsigned char c : str)
    {
        if (is_unreserved(c))
            result += static_cast<char>(c);
        else
        {
            result += '%';
            result += hex_chars[(c >> 4) & 0xF];
            result += hex_chars[c & 0xF];
        }
    }
    return result;
}

std::string Url::str_decode(std::string_view str)
{
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.size())
        {
            auto from_hex = [](char c) -> int {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                return -1;
            };
            int hi = from_hex(str[i + 1]);
            int lo = from_hex(str[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                result += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        result += str[i];
    }
    return result;
}

std::string Url::encode() const
{
    std::string result;
    if (!scheme.empty())
    {
        result += scheme;
        result += ':';
    }
    if (has_authority)
    {
        result += "//";
        if (!userinfo.empty())
        {
            result += userinfo;
            result += '@';
        }
        if (host.find(':') != std::string::npos)
        {
            result += '[';
            result += host;
            result += ']';
        }
        else
            result += host;
        if (port != 0)
        {
            result += ':';
            result += std::to_string(port);
        }
    }
    result += path;
    if (!query_params.empty())
    {
        result += '?';
        bool first = true;
        for (const auto & [key, value] : query_params)
        {
            if (!first)
                result += '&';
            result += Url::str_encode(key);
            result += '=';
            result += Url::str_encode(value);
            first = false;
        }
    }
    if (!fragment.empty())
    {
        result += '#';
        result += fragment;
    }
    return result;
}

} // namespace sihd::util
