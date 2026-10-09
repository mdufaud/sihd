#ifndef __SIHD_UTIL_URL_HPP__
#define __SIHD_UTIL_URL_HPP__

#include <map>
#include <string>
#include <string_view>

namespace sihd::util
{

// Syntactic URI parser/builder (RFC 3986), no protocol semantics here (SSL, default ports):
// that belongs to the consumer. decode: string -> Url, encode: Url -> string
struct Url
{
        Url() = default;
        // parse a URI string into components (same as Url::decode)
        explicit Url(std::string_view url);

        // string -> Url: split a URI string into its components
        static Url decode(std::string_view url);
        // Url -> string: rebuild the URI string from the components
        std::string encode() const;

        // percent-encoding of a single component (e.g. a query value): space -> %20
        static std::string str_encode(std::string_view str);
        // reverse of str_encode: %20 -> space
        static std::string str_decode(std::string_view str);

        // "scheme" before the first ':' (http, https, ftp, mailto...). May be empty.
        std::string scheme;
        // "userinfo" between "//" and '@' (user, user:pass). May be empty.
        std::string userinfo;
        // "host" — registered name or IP (IPv6 stored without the surrounding []).
        std::string host;
        // "port" after host ':'. 0 means none was given in the URI.
        int port = 0;
        // "path" hierarchical part after the authority ('/over/there').
        std::string path;
        // "query" after '?', parsed into key/value pairs (percent-decoded).
        std::map<std::string, std::string> query_params;
        // "fragment" after '#' ('nose'). May be empty.
        std::string fragment;
        // true if the URI had an authority component ("//..."). Distinguishes
        // "mailto:a@b" (no authority) from "file:///x" (empty authority).
        bool has_authority = false;
};

} // namespace sihd::util

#endif
