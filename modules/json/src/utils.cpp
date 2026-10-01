#include <simdjson.h>

#include <sihd/json/Json.hpp>
#include <sihd/json/utils.hpp>

namespace sihd::json::utils
{

namespace
{

std::string parse_error(simdjson::error_code code)
{
    return std::string("parse error: ") + simdjson::simdjson_error(code).what();
}

std::string key_not_found(std::string_view key)
{
    return "key '" + std::string(key) + "' not found";
}

} // namespace

std::expected<Json, std::string> find_first(std::string_view data, std::string_view key)
{
    simdjson::padded_string padded(data.data(), data.size());
    simdjson::ondemand::parser parser;

    auto doc_result = parser.iterate(padded);
    if (doc_result.error())
        return std::unexpected(parse_error(doc_result.error()));

    simdjson::ondemand::array arr;
    if (doc_result.value().get_array().get(arr) != simdjson::SUCCESS)
        return std::unexpected(std::string("document is not an array"));

    for (simdjson::ondemand::value elem : arr)
    {
        simdjson::ondemand::object obj;
        if (elem.get_object().get(obj) != simdjson::SUCCESS)
            break;

        simdjson::ondemand::value val;
        if (obj[key].get(val) != simdjson::SUCCESS)
            break;

        std::string_view raw;
        if (val.raw_json().get(raw) != simdjson::SUCCESS)
            break;

        // Json::parse copies the raw bytes into its own padded buffer
        auto parsed = Json::parse(raw);
        if (!parsed)
            return std::unexpected(std::move(parsed.error()));
        return std::move(*parsed);
    }
    return std::unexpected(key_not_found(key));
}

std::expected<size_t, std::string> for_each(std::string_view data, std::function<bool(Json)> callback)
{
    simdjson::padded_string padded(data.data(), data.size());
    simdjson::ondemand::parser parser;

    auto doc_result = parser.iterate(padded);
    if (doc_result.error())
        return std::unexpected(parse_error(doc_result.error()));

    simdjson::ondemand::array arr;
    if (doc_result.value().get_array().get(arr) != simdjson::SUCCESS)
        return std::unexpected(std::string("document is not an array"));

    size_t count = 0;
    for (simdjson::ondemand::value elem : arr)
    {
        auto raw_result = elem.raw_json();
        if (raw_result.error() != simdjson::SUCCESS)
            return std::unexpected(parse_error(raw_result.error()));
        std::string_view raw = raw_result.value();

        auto parsed = Json::parse(raw);
        if (!parsed)
            return std::unexpected(std::move(parsed.error()));
        ++count;
        if (!callback(std::move(*parsed)))
            break;
    }
    return count;
}

} // namespace sihd::json::utils
