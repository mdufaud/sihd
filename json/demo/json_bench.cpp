#include <simdjson.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <sihd/json/Json.hpp>
#include <sihd/json/utils.hpp>

namespace
{

using Clock = std::chrono::high_resolution_clock;
using Ns = std::chrono::nanoseconds;

double to_ms(Ns ns)
{
    return static_cast<double>(ns.count()) / 1'000'000.0;
}

// ─── Full Traversals (visit every node) ───
// DOM is expected to be faster here: it pre-parses everything in a single SIMD pass,
// then pointer-chasing is cheap.
// On-Demand parses token-by-token, so for full traversals it has more overhead.

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

// ─── Partial access: extract one field per object in a top-level array ───
// This still walks the whole top-level array, so it is only a moderate fit for
// On-Demand. The real sweet spot for On-Demand is early exit, benchmarked below.

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

// ─── File I/O ───

std::string read_file(const char *path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        std::fprintf(stderr, "cannot open: %s\n", path);
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// detect if JSON is array-of-objects (for partial benchmark)
std::string_view detect_first_key(simdjson::ondemand::parser & parser, simdjson::padded_string & padded)
{
    auto doc = parser.iterate(padded);
    if (doc.error() || doc.type().value() != simdjson::ondemand::json_type::array)
        return {};
    for (simdjson::ondemand::value elem : doc.get_array())
    {
        if (elem.type() == simdjson::ondemand::json_type::object)
        {
            for (simdjson::ondemand::field f : elem.get_object())
                return std::string_view(f.escaped_key());
        }
        break;
    }
    return {};
}

void bench_file(const char *path)
{
    std::printf("─────────────────────────────────────────────\n");
    std::printf("file: %s\n", path);

    auto t0 = Clock::now();
    std::string content = read_file(path);
    auto t1 = Clock::now();
    if (content.empty())
        return;

    const double read_ms = to_ms(t1 - t0);
    const double mb = content.size() / (1024.0 * 1024.0);
    std::printf("  file size : %zu bytes (%.1f MB)\n", content.size(), content.size() / (1024.0 * 1024.0));
    std::printf("  read      : %.3f ms\n", read_ms);

    constexpr int runs = 3;
    simdjson::padded_string padded(content.data(), content.size());

    std::printf("\n  === Full traversal (DOM should be fastest) ===\n");

    // ─── [simdjson DOM] raw parse + traverse ───
    {
        simdjson::dom::parser parser;
        Ns total {};
        size_t node_count = 0;
        bool failed = false;
        for (int i = 0; i < runs; ++i)
        {
            auto tp0 = Clock::now();
            auto result = parser.parse(padded);
            if (result.error())
            {
                failed = true;
                break;
            }
            node_count = traverse_dom(result.value());
            auto tp1 = Clock::now();
            total += tp1 - tp0;
        }
        if (failed)
            std::printf("  [simdjson DOM]  : FAILED\n");
        else
        {
            double ms = to_ms(total) / runs;
            std::printf("  [simdjson DOM]  : %8.3f ms  (%6.0f MB/s, %zu nodes)\n",
                        ms,
                        ms > 0 ? mb / (ms / 1000.0) : 0.0,
                        node_count);
        }
    }

    // ─── [simdjson OD] raw on-demand parse + traverse ───
    {
        simdjson::ondemand::parser parser;
        Ns total {};
        size_t node_count = 0;
        bool failed = false;
        for (int i = 0; i < runs; ++i)
        {
            auto tp0 = Clock::now();
            auto doc = parser.iterate(padded);
            if (doc.error())
            {
                failed = true;
                break;
            }
            node_count = traverse_ondemand_doc(doc.value());
            auto tp1 = Clock::now();
            total += tp1 - tp0;
        }
        if (failed)
            std::printf("  [simdjson OD]   : FAILED\n");
        else
        {
            double ms = to_ms(total) / runs;
            std::printf("  [simdjson OD]   : %8.3f ms  (%6.0f MB/s, %zu nodes)\n",
                        ms,
                        ms > 0 ? mb / (ms / 1000.0) : 0.0,
                        node_count);
        }
    }

    // ─── [Json] parse + traverse ───
    {
        Ns total {};
        size_t node_count = 0;
        bool failed = false;
        for (int i = 0; i < runs; ++i)
        {
            auto tp0 = Clock::now();
            auto parsed = sihd::json::Json::parse(content, false);
            if (parsed.is_discarded())
            {
                failed = true;
                break;
            }
            node_count = traverse_json(parsed);
            auto tp1 = Clock::now();
            total += tp1 - tp0;
        }
        if (failed)
            std::printf("  [Json]          : FAILED\n");
        else
        {
            double ms = to_ms(total) / runs;
            std::printf("  [Json]          : %8.3f ms  (%6.0f MB/s, %zu nodes)\n",
                        ms,
                        ms > 0 ? mb / (ms / 1000.0) : 0.0,
                        node_count);
        }
    }

    // ─── Partial access benchmark (OD should be fastest) ───
    {
        simdjson::ondemand::parser od_parser;
        std::string first_key_str;

        // detect if array-of-objects
        auto key_view = detect_first_key(od_parser, padded);
        if (!key_view.empty())
        {
            first_key_str = std::string(key_view);
            std::printf("\n  === Partial access: extract \"%s\" from each object ===\n",
                        first_key_str.c_str());

            // DOM partial
            {
                simdjson::dom::parser parser;
                Ns total {};
                size_t hit_count = 0;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto result = parser.parse(padded);
                    hit_count = partial_dom(result.value(), first_key_str);
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                double ms = to_ms(total) / runs;
                std::printf("  [DOM partial]   : %8.3f ms  (%6.0f MB/s, %zu hits)\n",
                            ms,
                            ms > 0 ? mb / (ms / 1000.0) : 0.0,
                            hit_count);
            }

            // OD partial
            {
                Ns total {};
                size_t hit_count = 0;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto doc = od_parser.iterate(padded);
                    hit_count = partial_ondemand(doc.value(), first_key_str);
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                double ms = to_ms(total) / runs;
                std::printf("  [OD partial]    : %8.3f ms  (%6.0f MB/s, %zu hits)\n",
                            ms,
                            ms > 0 ? mb / (ms / 1000.0) : 0.0,
                            hit_count);
            }

            // Json partial
            {
                Ns total {};
                size_t hit_count = 0;
                bool failed = false;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto parsed = sihd::json::Json::parse(content, false);
                    if (parsed.is_discarded())
                    {
                        failed = true;
                        break;
                    }
                    hit_count = partial_json(parsed, first_key_str);
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                if (failed)
                    std::printf("  [Json part]     : FAILED\n");
                else
                {
                    double ms = to_ms(total) / runs;
                    std::printf("  [Json part]     : %8.3f ms  (%6.0f MB/s, %zu hits)\n",
                                ms,
                                ms > 0 ? mb / (ms / 1000.0) : 0.0,
                                hit_count);
                }
            }

            std::printf("\n  === Early exit: extract \"%s\" from first object only ===\n",
                        first_key_str.c_str());

            // DOM first object only
            {
                simdjson::dom::parser parser;
                Ns total {};
                bool hit = false;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto result = parser.parse(padded);
                    hit = first_field_dom(result.value(), first_key_str);
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                double ms = to_ms(total) / runs;
                std::printf("  [DOM first]     : %8.3f ms  (%6.0f MB/s, %s)\n",
                            ms,
                            ms > 0 ? mb / (ms / 1000.0) : 0.0,
                            hit ? "hit" : "miss");
            }

            // OD first object only
            {
                Ns total {};
                bool hit = false;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto doc = od_parser.iterate(padded);
                    hit = first_field_ondemand(doc.value(), first_key_str);
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                double ms = to_ms(total) / runs;
                std::printf("  [OD first]      : %8.3f ms  (%6.0f MB/s, %s)\n",
                            ms,
                            ms > 0 ? mb / (ms / 1000.0) : 0.0,
                            hit ? "hit" : "miss");
            }

            // od::find_first — OD early-exit returning Json
            {
                Ns total {};
                bool hit = false;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto result = sihd::json::utils::find_first(content, first_key_str);
                    hit = result.has_value() && !result->is_null();
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                double ms = to_ms(total) / runs;
                std::printf("  [od::find_first]: %8.3f ms  (%6.0f MB/s, %s)\n",
                            ms,
                            ms > 0 ? mb / (ms / 1000.0) : 0.0,
                            hit ? "hit" : "miss");
            }

            // Json first object only
            {
                Ns total {};
                bool hit = false;
                bool failed = false;
                for (int i = 0; i < runs; ++i)
                {
                    auto tp0 = Clock::now();
                    auto parsed = sihd::json::Json::parse(content, false);
                    if (parsed.is_discarded())
                    {
                        failed = true;
                        break;
                    }
                    hit = first_field_json(parsed, first_key_str);
                    auto tp1 = Clock::now();
                    total += tp1 - tp0;
                }
                if (failed)
                    std::printf("  [Json first]    : FAILED\n");
                else
                {
                    double ms = to_ms(total) / runs;
                    std::printf("  [Json first]    : %8.3f ms  (%6.0f MB/s, %s)\n",
                                ms,
                                ms > 0 ? mb / (ms / 1000.0) : 0.0,
                                hit ? "hit" : "miss");
                }
            }
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: json_bench <file.json> [file2.json ...]\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i)
        bench_file(argv[i]);

    std::printf("─────────────────────────────────────────────\n");
    return 0;
}
