#ifndef __SIHD_JSON_JSON_HPP__
#define __SIHD_JSON_JSON_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <simdjson.h>

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

        // construction

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

        // copy / move

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

        // element access

        Json operator[](std::string_view key) const;
        Json operator[](size_t index) const;

        size_t size() const;
        bool empty() const;

        // value extraction

        template <typename T>
        T get() const;

        // parsing

        static Json parse(std::string_view str);
        static Json parse(std::string_view str, bool allow_exceptions);
        static Json parse(const char *begin, const char *end, void *callback, bool allow_exceptions);

        // serialization

        std::string dump(int indent = -1) const;

        // DOM backing: kept after parse() for lazy access via simdjson
        struct DomHolder
        {
                simdjson::dom::parser parser;
                simdjson::padded_string source;
        };

        // iteration (defined after Json is complete to allow inline Json member)

        class iterator;

        iterator begin() const;
        iterator end() const;

    private:
        friend class iterator;

        struct DiscardedTag
        {
        };
        Json(DiscardedTag);

        mutable Value _value;
        bool _discarded;
        std::shared_ptr<DomHolder> _dom_holder;
        simdjson::dom::element _dom_element {};

        void _dump_to_builder(simdjson::builder::string_builder & sb, int indent, int depth) const;
        static void _write_indent(simdjson::builder::string_builder & sb, int indent, int depth);
};

// Iterator defined outside Json so that Json is a complete type.

class Json::iterator
{
    public:
        iterator() = default;

        const std::string & key() const;

        const Json & value() const;
        const Json & operator*() const;
        const Json *operator->() const;

        iterator & operator++();

        iterator operator++(int);

        bool operator==(const iterator & other) const;
        bool operator!=(const iterator & other) const;

    private:
        friend class Json;

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

        // DOM end sentinel (lightweight, no shared_ptr copy)
        struct EndTag
        {
        };
        iterator(EndTag, IterType type, simdjson::dom::array::iterator arr_end);
        iterator(EndTag, IterType type, simdjson::dom::object::iterator obj_end);

        IterType _iter_type = IterType::Array;
        size_t _index = 0;
        const void *_container = nullptr;

        // DOM iteration (zero heap allocation)
        bool _dom_mode = false;
        simdjson::dom::array::iterator _arr_it {};
        simdjson::dom::object::iterator _obj_it {};
        mutable Json _current;
        mutable std::string _current_key;
};

// template specializations declared here, defined in .cpp

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

} // namespace sihd::json

#endif
