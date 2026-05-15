#include <simdjson.h>

#include <sihd/json/Json.hpp>
#include <sihd/json/utils.hpp>

namespace sihd::json::utils
{

std::optional<Json> find_first(std::string_view data, std::string_view key)
{
    simdjson::padded_string padded(data.data(), data.size());
    simdjson::ondemand::parser parser;

    auto doc_result = parser.iterate(padded);
    if (doc_result.error())
        return std::nullopt;

    simdjson::ondemand::array arr;
    if (doc_result.value().get_array().get(arr) != simdjson::SUCCESS)
        return std::nullopt;

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
        return Json::parse(raw, false);
    }
    return std::nullopt;
}

void for_each(std::string_view data, std::function<bool(Json)> callback)
{
    simdjson::padded_string padded(data.data(), data.size());
    simdjson::ondemand::parser parser;

    auto doc_result = parser.iterate(padded);
    if (doc_result.error())
        return;

    simdjson::ondemand::array arr;
    if (doc_result.value().get_array().get(arr) != simdjson::SUCCESS)
        return;

    for (simdjson::ondemand::value elem : arr)
    {
        std::string_view raw;
        if (elem.raw_json().get(raw) != simdjson::SUCCESS)
            continue;

        if (!callback(Json::parse(raw, false)))
            return;
    }
}

} // namespace sihd::json::utils
