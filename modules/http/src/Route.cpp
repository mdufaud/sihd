#include <algorithm>

#include <sihd/http/Route.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/util/str.hpp>

namespace sihd::http
{

namespace
{

// the allow header lists methods in a stable order
constexpr HttpRequest::RequestType allowed_method_order[] = {
    HttpRequest::Get,
    HttpRequest::Post,
    HttpRequest::Put,
    HttpRequest::Delete,
    HttpRequest::Patch,
    HttpRequest::Head,
    HttpRequest::Options,
};

} // namespace

std::string normalize_path(std::string_view raw)
{
    auto segments = sihd::util::str::split(raw, '/');
    if (segments.empty())
        return "";
    for (auto & seg : segments)
        seg = sihd::util::Url::str_decode(seg);
    return sihd::util::str::join(std::span<const std::string>(segments), "/");
}

Route::Route(std::string_view pattern, Handler handler):
    _pattern(pattern),
    _has_params(false),
    _has_catch_all(false),
    _specificity(0),
    _handler(std::move(handler))
{
    auto parts = sihd::util::str::split(pattern, '/');
    _segments.reserve(parts.size());
    for (const auto & part : parts)
    {
        if (part.size() >= 3 && part.front() == '{' && part.back() == '}')
        {
            std::string_view name(part);
            name = name.substr(1, name.size() - 2);
            bool catch_all = false;
            if (name.size() >= 4 && sihd::util::str::ends_with(name, "..."))
            {
                name = name.substr(0, name.size() - 3);
                catch_all = true;
                _has_catch_all = true;
            }
            _segments.push_back({std::string(name), true, catch_all});
            _has_params = true;
        }
        else
        {
            _segments.push_back({part, false, false});
            ++_specificity;
        }
    }
}

RouteMatch Route::match(std::string_view path) const
{
    auto parts = sihd::util::str::split(path, '/');
    RouteMatch result;

    if (_has_catch_all)
    {
        if (parts.size() < _segments.size())
            return result;
    }
    else
    {
        if (parts.size() != _segments.size())
            return result;
    }

    for (size_t i = 0; i < _segments.size(); ++i)
    {
        if (_segments[i].is_catch_all)
        {
            std::string joined;
            for (size_t j = i; j < parts.size(); ++j)
            {
                if (!joined.empty())
                    joined += '/';
                joined += sihd::util::Url::str_decode(parts[j]);
            }
            result.params[_segments[i].value] = std::move(joined);
            result.matched = true;
            return result;
        }
        else if (_segments[i].is_param)
        {
            result.params[_segments[i].value] = sihd::util::Url::str_decode(parts[i]);
        }
        else if (_segments[i].value != sihd::util::Url::str_decode(parts[i]))
        {
            return RouteMatch {};
        }
    }
    result.matched = true;
    return result;
}

void RouteTable::add(std::string_view pattern, Route::Handler handler, HttpRequest::RequestType method)
{
    _routes[static_cast<int>(method)].emplace_back(pattern, std::move(handler));
}

std::optional<RouteTable::FindResult> RouteTable::find(HttpRequest::RequestType method, std::string_view path) const
{
    auto it = _routes.find(static_cast<int>(method));
    if (it == _routes.end())
        return std::nullopt;

    std::string normalized = normalize_path(path);

    // exact routes first (no params, no catch-all)
    for (const auto & route : it->second)
    {
        if (route.is_parameterized() == false)
        {
            auto m = route.match(normalized);
            if (m.matched)
                return FindResult {route.handler(), std::move(m)};
        }
    }

    // parameterized routes sorted by specificity (most literal segments first)
    // catch-all routes last
    std::vector<const Route *> param_routes;
    for (const auto & route : it->second)
    {
        if (route.is_parameterized())
            param_routes.push_back(&route);
    }
    std::stable_sort(param_routes.begin(), param_routes.end(), [](const Route *a, const Route *b) {
        if (a->has_catch_all() != b->has_catch_all())
            return !a->has_catch_all();
        return a->specificity() > b->specificity();
    });

    for (const auto *route : param_routes)
    {
        auto m = route->match(normalized);
        if (m.matched)
            return FindResult {route->handler(), std::move(m)};
    }
    return std::nullopt;
}

std::vector<HttpRequest::RequestType> RouteTable::allowed_methods(std::string_view path) const
{
    const std::string normalized = normalize_path(path);
    std::vector<HttpRequest::RequestType> allowed;

    for (HttpRequest::RequestType method : allowed_method_order)
    {
        const auto it = _routes.find(static_cast<int>(method));
        if (it == _routes.end())
            continue;
        for (const Route & route : it->second)
        {
            if (route.match(normalized).matched)
            {
                allowed.push_back(method);
                break;
            }
        }
    }
    return allowed;
}

} // namespace sihd::http
