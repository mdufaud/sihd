#ifndef __SIHD_HTTP_ROUTE_HPP__
#define __SIHD_HTTP_ROUTE_HPP__

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>

namespace sihd::http
{

struct RouteMatch
{
        bool matched = false;
        std::unordered_map<std::string, std::string> params;
};

class Route
{
    public:
        using Handler = std::function<void(const HttpRequest &, HttpResponse &)>;

        Route(std::string_view pattern, Handler handler);
        ~Route() = default;

        RouteMatch match(std::string_view path) const;

        bool is_parameterized() const { return _has_params; }
        bool has_catch_all() const { return _has_catch_all; }
        const std::string & pattern() const { return _pattern; }
        // number of literal (non-param) segments — higher = more specific
        size_t specificity() const { return _specificity; }

    private:
        struct Segment
        {
                std::string value;
                bool is_param;
                bool is_catch_all;
        };

        std::string _pattern;
        std::vector<Segment> _segments;
        bool _has_params;
        bool _has_catch_all;
        size_t _specificity;
        Handler _handler;

        friend class RouteTable;
};

// normalize a URL path: strip trailing slash, collapse double slashes, percent-decode
std::string normalize_path(std::string_view raw);

class RouteTable
{
    public:
        RouteTable() = default;
        ~RouteTable() = default;

        void add(std::string_view pattern,
                 Route::Handler handler,
                 HttpRequest::RequestType method = HttpRequest::Get);

        struct FindResult
        {
                Route::Handler handler;
                RouteMatch match;
        };

        std::optional<FindResult> find(HttpRequest::RequestType method, std::string_view path) const;

    private:
        std::unordered_map<int, std::vector<Route>> _routes;
};

} // namespace sihd::http

#endif
