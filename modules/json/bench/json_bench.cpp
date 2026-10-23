#include <simdjson.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <sihd/json/Json.hpp>
#include <sihd/json/utils.hpp>

#include <benchmark/benchmark.h>

namespace
{

size_t traverse_json(const sihd::json::Json & j)
{
    switch (j.type())
    {
        case sihd::json::Json::Type::Array:
        case sihd::json::Json::Type::Object:
        {
            size_t count = 1;
            for (const auto & child : j)
                count += traverse_json(child);
            return count;
        }
        default:
            return 1;
    }
}

size_t traverse_ondemand(simdjson::ondemand::value val)
{
    switch (val.type())
    {
        case simdjson::ondemand::json_type::array:
        {
            size_t count = 1;
            for (simdjson::ondemand::value child : val.get_array())
                count += traverse_ondemand(child);
            return count;
        }
        case simdjson::ondemand::json_type::object:
        {
            size_t count = 1;
            for (simdjson::ondemand::field field : val.get_object())
            {
                field.escaped_key();
                count += traverse_ondemand(field.value());
            }
            return count;
        }
        default:
            return 1;
    }
}

size_t traverse_ondemand_doc(simdjson::ondemand::document & doc)
{
    switch (doc.type())
    {
        case simdjson::ondemand::json_type::array:
        {
            size_t count = 1;
            for (simdjson::ondemand::value child : doc.get_array())
                count += traverse_ondemand(child);
            return count;
        }
        case simdjson::ondemand::json_type::object:
        {
            size_t count = 1;
            for (simdjson::ondemand::field field : doc.get_object())
            {
                field.escaped_key();
                count += traverse_ondemand(field.value());
            }
            return count;
        }
        default:
            return 1;
    }
}

size_t traverse_dom(simdjson::dom::element elem)
{
    switch (elem.type())
    {
        case simdjson::dom::element_type::ARRAY:
        {
            size_t count = 1;
            for (auto child : simdjson::dom::array(elem))
                count += traverse_dom(child);
            return count;
        }
        case simdjson::dom::element_type::OBJECT:
        {
            size_t count = 1;
            for (auto field : simdjson::dom::object(elem))
                count += traverse_dom(field.value);
            return count;
        }
        default:
            return 1;
    }
}

size_t partial_ondemand(simdjson::ondemand::document & doc, std::string_view field_name)
{
    size_t count = 0;
    for (simdjson::ondemand::object obj : doc.get_array())
    {
        simdjson::ondemand::value val;
        if (obj[field_name].get(val) == simdjson::SUCCESS)
            count++;
    }
    return count;
}

size_t partial_dom(simdjson::dom::element doc, std::string_view field_name)
{
    size_t count = 0;
    for (auto elem : simdjson::dom::array(doc))
    {
        simdjson::dom::element val;
        if (simdjson::dom::object(elem)[field_name].get(val) == simdjson::SUCCESS)
            count++;
    }
    return count;
}

size_t partial_json(const sihd::json::Json & doc, std::string_view field_name)
{
    size_t count = 0;
    for (const auto & elem : doc)
    {
        if (!elem[field_name].is_null())
            count++;
    }
    return count;
}

bool first_field_ondemand(simdjson::ondemand::document & doc, std::string_view field_name)
{
    for (simdjson::ondemand::object obj : doc.get_array())
    {
        simdjson::ondemand::value val;
        return obj[field_name].get(val) == simdjson::SUCCESS;
    }
    return false;
}

bool first_field_dom(simdjson::dom::element doc, std::string_view field_name)
{
    for (auto elem : simdjson::dom::array(doc))
    {
        simdjson::dom::element val;
        return simdjson::dom::object(elem)[field_name].get(val) == simdjson::SUCCESS;
    }
    return false;
}

bool first_field_json(const sihd::json::Json & doc, std::string_view field_name)
{
    for (const auto & elem : doc)
        return !elem[field_name].is_null();
    return false;
}

std::string read_file(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string_view detect_first_key(simdjson::ondemand::parser & parser, simdjson::padded_string & padded)
{
    auto doc = parser.iterate(padded);
    if (doc.error() || doc.type().value() != simdjson::ondemand::json_type::array)
        return {};
    for (simdjson::ondemand::value elem : doc.get_array())
    {
        if (elem.type() == simdjson::ondemand::json_type::object)
        {
            for (simdjson::ondemand::field field : elem.get_object())
                return std::string_view(field.escaped_key());
        }
        break;
    }
    return {};
}

std::string generate_records(size_t count)
{
    std::string out = "[";
    for (size_t i = 0; i < count; ++i)
    {
        if (i > 0)
            out += ',';
        out += "{\"id\":" + std::to_string(i) + ",\"name\":\"item-" + std::to_string(i)
               + "\",\"score\":" + std::to_string(i * 0.5) + ",\"active\":" + (i % 2 ? "true" : "false")
               + ",\"tags\":[\"a\",\"b\",\"c\"],\"pos\":{\"x\":" + std::to_string(i % 1000)
               + ",\"y\":" + std::to_string(i / 1000) + "}}";
    }
    out += "]";
    return out;
}

struct Payload
{
        std::string label;
        std::string content;
        simdjson::padded_string padded;
        std::string first_key;
};

// SIHD_JSON_BENCH_FILE benches that document instead of the generated records
Payload & payload()
{
    static Payload instance = [] {
        Payload loaded;
        const char *path = std::getenv("SIHD_JSON_BENCH_FILE");
        loaded.label = path != nullptr ? path : "generated records";
        loaded.content = path != nullptr ? read_file(path) : generate_records(100000);
        loaded.padded = simdjson::padded_string(loaded.content.data(), loaded.content.size());
        simdjson::ondemand::parser parser;
        loaded.first_key = std::string(detect_first_key(parser, loaded.padded));
        return loaded;
    }();
    return instance;
}

// query parses the payload and returns what it counted, nullopt when parsing failed
template <typename Query>
void bench_payload(benchmark::State & state, const char *counter, bool needs_key, Query && query)
{
    Payload & current = payload();
    if (current.content.empty())
    {
        state.SkipWithError("empty payload: " + current.label);
        return;
    }
    if (needs_key && current.first_key.empty())
    {
        state.SkipWithError("not an array of objects: " + current.label);
        return;
    }

    size_t counted = 0;
    bool ok = true;
    for (auto _ : state)
    {
        std::optional<size_t> res = query(current);
        if (!res)
        {
            state.SkipWithError("parse failed: " + current.label);
            ok = false;
            break;
        }
        counted = *res;
        benchmark::DoNotOptimize(counted);
    }
    if (!ok)
        return;
    state.SetLabel(current.label);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(current.content.size()));
    state.counters[counter] = static_cast<double>(counted);
}

void BM_traverse_simdjson_dom(benchmark::State & state)
{
    simdjson::dom::parser parser;
    bench_payload(state, "nodes", false, [&parser](Payload & p) -> std::optional<size_t> {
        auto result = parser.parse(p.padded);
        if (result.error())
            return std::nullopt;
        return traverse_dom(result.value());
    });
}

void BM_traverse_simdjson_ondemand(benchmark::State & state)
{
    simdjson::ondemand::parser parser;
    bench_payload(state, "nodes", false, [&parser](Payload & p) -> std::optional<size_t> {
        auto doc = parser.iterate(p.padded);
        if (doc.error())
            return std::nullopt;
        return traverse_ondemand_doc(doc.value());
    });
}

void BM_traverse_json(benchmark::State & state)
{
    bench_payload(state, "nodes", false, [](Payload & p) -> std::optional<size_t> {
        auto parsed = sihd::json::Json::parse(p.content);
        if (!parsed)
            return std::nullopt;
        return traverse_json(*parsed);
    });
}

void BM_partial_simdjson_dom(benchmark::State & state)
{
    simdjson::dom::parser parser;
    bench_payload(state, "hits", true, [&parser](Payload & p) -> std::optional<size_t> {
        auto result = parser.parse(p.padded);
        if (result.error())
            return std::nullopt;
        return partial_dom(result.value(), p.first_key);
    });
}

void BM_partial_simdjson_ondemand(benchmark::State & state)
{
    simdjson::ondemand::parser parser;
    bench_payload(state, "hits", true, [&parser](Payload & p) -> std::optional<size_t> {
        auto doc = parser.iterate(p.padded);
        if (doc.error())
            return std::nullopt;
        return partial_ondemand(doc.value(), p.first_key);
    });
}

void BM_partial_json(benchmark::State & state)
{
    bench_payload(state, "hits", true, [](Payload & p) -> std::optional<size_t> {
        auto parsed = sihd::json::Json::parse(p.content);
        if (!parsed)
            return std::nullopt;
        return partial_json(*parsed, p.first_key);
    });
}

void BM_first_simdjson_dom(benchmark::State & state)
{
    simdjson::dom::parser parser;
    bench_payload(state, "hit", true, [&parser](Payload & p) -> std::optional<size_t> {
        auto result = parser.parse(p.padded);
        if (result.error())
            return std::nullopt;
        return first_field_dom(result.value(), p.first_key);
    });
}

void BM_first_simdjson_ondemand(benchmark::State & state)
{
    simdjson::ondemand::parser parser;
    bench_payload(state, "hit", true, [&parser](Payload & p) -> std::optional<size_t> {
        auto doc = parser.iterate(p.padded);
        if (doc.error())
            return std::nullopt;
        return first_field_ondemand(doc.value(), p.first_key);
    });
}

void BM_first_find_first(benchmark::State & state)
{
    bench_payload(state, "hit", true, [](Payload & p) -> std::optional<size_t> {
        auto result = sihd::json::utils::find_first(p.content, p.first_key);
        return result.has_value() && !result->is_null();
    });
}

void BM_first_json(benchmark::State & state)
{
    bench_payload(state, "hit", true, [](Payload & p) -> std::optional<size_t> {
        auto parsed = sihd::json::Json::parse(p.content);
        if (!parsed)
            return std::nullopt;
        return first_field_json(*parsed, p.first_key);
    });
}

sihd::json::Json build_records(size_t count)
{
    sihd::json::Json::Array records;
    records.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        sihd::json::Json::Object pos;
        pos.emplace_back("x", static_cast<int64_t>(i % 1000));
        pos.emplace_back("y", static_cast<int64_t>(i / 1000));

        sihd::json::Json::Object record;
        record.emplace_back("id", static_cast<int64_t>(i));
        record.emplace_back("name", "item-" + std::to_string(i));
        // never integral: a dumped 1.0 could parse back as an integer
        record.emplace_back("score", static_cast<double>(i) + 0.5);
        record.emplace_back("active", i % 2 == 1);
        record.emplace_back("tags", sihd::json::Json::Array {"a", "b", "c"});
        record.emplace_back("pos", std::move(pos));
        records.emplace_back(std::move(record));
    }
    return sihd::json::Json(std::move(records));
}

void BM_build_records(benchmark::State & state)
{
    const size_t count = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        sihd::json::Json built = build_records(count);
        benchmark::DoNotOptimize(built);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

// dump checks: once outside the timing, the dump parses back to the same document
void bench_dump(benchmark::State & state, const sihd::json::Json & doc)
{
    const std::string dumped = doc.dump();
    const auto parsed_back = sihd::json::Json::parse(dumped);
    if (!parsed_back || !(*parsed_back == doc))
    {
        state.SkipWithError("the dump does not parse back to the document");
        return;
    }
    for (auto _ : state)
    {
        std::string out = doc.dump();
        benchmark::DoNotOptimize(out.data());
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(dumped.size()));
}

void BM_dump_built(benchmark::State & state)
{
    bench_dump(state, build_records(static_cast<size_t>(state.range(0))));
}

void BM_dump_parsed(benchmark::State & state)
{
    Payload & current = payload();
    const auto parsed = sihd::json::Json::parse(current.content);
    if (!parsed)
    {
        state.SkipWithError("parse failed: " + current.label);
        return;
    }
    state.SetLabel(current.label);
    bench_dump(state, *parsed);
}

// a ~1 KB message: per call costs (parser, padded copy) weigh more than the parsing itself
const std::string & small_document()
{
    static const std::string document = generate_records(8);
    return document;
}

void BM_parse_small_json(benchmark::State & state)
{
    const std::string & document = small_document();
    for (auto _ : state)
    {
        auto parsed = sihd::json::Json::parse(document);
        if (!parsed)
        {
            state.SkipWithError("parse failed");
            break;
        }
        benchmark::DoNotOptimize(parsed);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(document.size()));
}

// reused parser and padded input: the floor under Json::parse
void BM_parse_small_simdjson_dom(benchmark::State & state)
{
    const simdjson::padded_string padded(small_document());
    simdjson::dom::parser parser;
    for (auto _ : state)
    {
        auto result = parser.parse(padded);
        if (result.error())
        {
            state.SkipWithError("parse failed");
            break;
        }
        benchmark::DoNotOptimize(result.value());
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(padded.size()));
}

// full traversal: every node visited
BENCHMARK(BM_traverse_simdjson_dom)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_traverse_simdjson_ondemand)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_traverse_json)->Unit(benchmark::kMillisecond);
// partial access: the first key of every object
BENCHMARK(BM_partial_simdjson_dom)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_partial_simdjson_ondemand)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_partial_json)->Unit(benchmark::kMillisecond);
// early exit: the first key of the first object only
BENCHMARK(BM_first_simdjson_dom)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_first_simdjson_ondemand)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_first_find_first)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_first_json)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_build_records)->ArgName("records")->Arg(8)->Arg(1000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_dump_built)->ArgName("records")->Arg(8)->Arg(1000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_dump_parsed)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_parse_small_json);
BENCHMARK(BM_parse_small_simdjson_dom);

} // namespace
