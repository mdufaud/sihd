#ifndef __SIHD_JSON_JSON_HPP__
#define __SIHD_JSON_JSON_HPP__

#include <simdjson.h>

#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace sihd::json
{

class Json
{
    public:
        using Array = std::vector<Json>;
        using Object = std::vector<std::pair<std::string, Json>>;
        using ArrayPtr = std::unique_ptr<Array>;
        using ObjectPtr = std::unique_ptr<Object>;
        using Value = std::variant<std::nullptr_t, bool, int64_t, uint64_t, double, std::string, ArrayPtr, ObjectPtr>;

        enum class Type
        {
            Null,
            Bool,
            Int,
            Uint,
            Float,
            String,
            Array,
            Object,
            Discarded,
        };

        Json();
        Json(std::nullptr_t);
        Json(bool val);
        Json(int32_t val);
        Json(uint32_t val);
        Json(int64_t val);
        Json(uint64_t val);
        Json(float val);
        Json(double val);
        Json(const char *val);
        Json(std::string_view val);
        Json(std::string val);
        Json(Array && arr);
        Json(Object && obj);
        Json(std::initializer_list<Json> init);

        Json(const Json & other);
        Json(Json && other) noexcept;
        Json & operator=(const Json & other);
        Json & operator=(Json && other) noexcept;

        ~Json();

        bool is_null() const;
        bool is_bool() const;
        bool is_number_integer() const;
        bool is_number_unsigned() const;
        bool is_number_float() const;
        bool is_number() const;
        bool is_string() const;
        bool is_array() const;
        bool is_object() const;
        bool is_discarded() const;

        Type type() const;

        Json operator[](std::string_view key) const;
        Json operator[](size_t index) const;

        size_t size() const;
        bool empty() const;
        bool contains(std::string_view key) const;

        template <typename T>
        T get() const;

        template <typename T>
        T get_or(T default_value) const
        {
            try
            {
                return get<T>();
            }
            catch (const std::runtime_error &)
            {
                return default_value;
            }
        }

        static std::expected<Json, std::string> parse(std::string_view str);
        static std::expected<Json, std::string> parse(const char *begin, const char *end);

        std::string dump(int indent = -1) const;

        bool operator==(const Json & other) const;

        class iterator;

        iterator begin() const;
        iterator end() const;

    private:
        struct DomHolder
        {
                simdjson::dom::parser parser;
                simdjson::padded_string source;
        };

        struct DiscardedTag
        {
        };
        Json(DiscardedTag);

        Value _value;
        bool _discarded;
        std::shared_ptr<DomHolder> _dom_holder;
        simdjson::dom::element _dom_element {};

        void _dump_to_builder(simdjson::builder::string_builder & sb, int indent, int depth) const;
        static void _dump_dom_to_builder(simdjson::builder::string_builder & sb,
                                         simdjson::dom::element elem,
                                         int indent,
                                         int depth);
        static void _write_indent(simdjson::builder::string_builder & sb, int indent, int depth);
};

// Json must be complete: the iterator holds one inline

class Json::iterator
{
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Json;
        using difference_type = std::ptrdiff_t;
        using pointer = const Json *;
        using reference = const Json &;

        iterator() = default;

        static iterator begin(const Json & json);
        static iterator end(const Json & json);

        const std::string & key() const;

        const Json & value() const;
        const Json & operator*() const;
        const Json *operator->() const;

        iterator & operator++();

        iterator operator++(int);

        bool operator==(const iterator & other) const;
        bool operator!=(const iterator & other) const;

    private:
        enum class IterType
        {
            Array,
            Object,
        };

        iterator(IterType type, size_t index, const void *container);

        iterator(simdjson::dom::array::iterator arr_it,
                 simdjson::dom::array::iterator arr_end,
                 const std::shared_ptr<DomHolder> & holder);

        iterator(simdjson::dom::object::iterator obj_it,
                 simdjson::dom::object::iterator obj_end,
                 const std::shared_ptr<DomHolder> & holder);

        // end sentinel, no shared_ptr copy
        struct EndTag
        {
        };
        iterator(EndTag, IterType type, simdjson::dom::array::iterator arr_end);
        iterator(EndTag, IterType type, simdjson::dom::object::iterator obj_end);

        IterType _iter_type = IterType::Array;
        size_t _index = 0;
        const void *_container = nullptr;

        bool _dom_mode = false;
        simdjson::dom::array::iterator _arr_it {};
        simdjson::dom::object::iterator _obj_it {};
        mutable Json _current;
        mutable std::string _current_key;
};

template <>
bool Json::get<bool>() const;
template <>
int8_t Json::get<int8_t>() const;
template <>
uint8_t Json::get<uint8_t>() const;
template <>
int16_t Json::get<int16_t>() const;
template <>
uint16_t Json::get<uint16_t>() const;
template <>
int32_t Json::get<int32_t>() const;
template <>
uint32_t Json::get<uint32_t>() const;
template <>
int64_t Json::get<int64_t>() const;
template <>
uint64_t Json::get<uint64_t>() const;
template <>
float Json::get<float>() const;
template <>
double Json::get<double>() const;
template <>
std::string Json::get<std::string>() const;
template <>
std::vector<std::string> Json::get<std::vector<std::string>>() const;

} // namespace sihd::json

#endif
