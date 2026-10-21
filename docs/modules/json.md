# json

JSON document parsing, typed access and serialization. Parsed documents are backed by simdjson's DOM, so values read from a parsed document reference its shared buffer; a `Json` built in code owns its data. A separate on-demand helper scans large top-level arrays and objects without building the full DOM.

| | |
|---|---|
| Depends on | none |
| External libraries | simdjson |
| Platforms | all |
| Build | `make m=json` |
| Header | `#include <sihd/json/Json.hpp>` |

## Capabilities

### Parsing and serialization

- `Json::parse(std::string_view)` and `Json::parse(begin, end)`: parse a document, returns `std::expected<Json, std::string>`.
- `Json::dump(int indent = -1)`: serialize to a string; a negative indent produces compact output.

### Construction

- `Json` constructors: from `nullptr`, `bool`, signed and unsigned integers, `float`, `double`, strings, `Json::Array`, `Json::Object` and initializer lists.
- `Json::Array` (`std::vector<Json>`) and `Json::Object` (`std::vector<std::pair<std::string, Json>>`): containers used to build arrays and objects.

### Type inspection

- `type()`: returns a `Json::Type` (`Null`, `Bool`, `Int`, `Uint`, `Float`, `String`, `Array`, `Object`, `Discarded`).
- `is_null`, `is_bool`, `is_number`, `is_number_integer`, `is_number_unsigned`, `is_number_float`, `is_string`, `is_array`, `is_object`, `is_discarded`: type predicates.

### Access

- `operator[](std::string_view key)` and `operator[](size_t index)`: child lookup; a missing key or index, or a wrong container type, yields a null `Json`.
- `contains(key)`, `size()`, `empty()`: object membership and element count.
- `get<T>()`: typed value for `bool`, `int8_t` to `int64_t`, `uint8_t` to `uint64_t`, `float`, `double`, `std::string` and `std::vector<std::string>`; throws `std::runtime_error` on a type mismatch.
- `get_or<T>(default)`: same as `get<T>()`, returns `default` instead of throwing.
- `begin()` / `end()` and `Json::iterator`: iterate array elements or object members (`key()`, `value()`).
- `operator==`: structural comparison.

### Lazy search

- `utils::find_first(data, key)`: returns the value of the first top-level object key, stopping the scan early; an absent key is an error.
- `utils::for_each(data, callback)`: walks a top-level array element by element; the callback returns `false` to stop early and the function returns the number of elements walked.

## Example

Adapted from `modules/json/test/TestJson.cpp`.

```cpp
#include <sihd/json/Json.hpp>

using sihd::json::Json;

void read_config()
{
    auto res = Json::parse(R"({"name": "test", "value": 42, "active": true})");
    if (!res)
        return;

    const Json & j = *res;
    if (!j.is_object())
        return;

    std::string name = j["name"].get<std::string>();     // "test"
    int32_t value = j["value"].get<int32_t>();           // 42
    bool active = j["active"].get<bool>();               // true
    int32_t retries = j["retries"].get_or<int32_t>(3);   // 3, key absent
    std::string text = j.dump(2);                        // indented output
}
```

## Scripting

Exposed to Lua (requires `lua=1`, module `sihd.json`). Not exposed to Python.
