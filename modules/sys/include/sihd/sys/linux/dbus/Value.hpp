#ifndef __SIHD_SYS_LINUX_DBUS_VALUE_HPP__
#define __SIHD_SYS_LINUX_DBUS_VALUE_HPP__

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::linux_::dbus
{

template <typename T>
concept BasicType = std::same_as<T, bool> || std::same_as<T, uint8_t> || std::same_as<T, int16_t>
                    || std::same_as<T, uint16_t> || std::same_as<T, int32_t> || std::same_as<T, uint32_t>
                    || std::same_as<T, int64_t> || std::same_as<T, uint64_t> || std::same_as<T, double>
                    || std::same_as<T, std::string>;

template <typename T>
concept FixedType = BasicType<T> && !std::same_as<T, std::string>;

// the dbus wire type codes, libdbus not being there on every platform
template <BasicType T>
constexpr char dbus_type_of()
{
    if constexpr (std::is_same_v<T, bool>)
        return 'b';
    else if constexpr (std::is_same_v<T, uint8_t>)
        return 'y';
    else if constexpr (std::is_same_v<T, int16_t>)
        return 'n';
    else if constexpr (std::is_same_v<T, uint16_t>)
        return 'q';
    else if constexpr (std::is_same_v<T, int32_t>)
        return 'i';
    else if constexpr (std::is_same_v<T, uint32_t>)
        return 'u';
    else if constexpr (std::is_same_v<T, int64_t>)
        return 'x';
    else if constexpr (std::is_same_v<T, uint64_t>)
        return 't';
    else if constexpr (std::is_same_v<T, std::string>)
        return 's';
    else
        return 'd';
}

// a dbus value whose signature comes from its C++ type: an int literal is 'i', write uint32_t {5} for a 'u'
class Value
{
    public:
        class ObjectPath
        {
            public:
                explicit ObjectPath(const std::string & path);

                const std::string & str() const;

            private:
                std::string _path;
        };

        class Signature
        {
            public:
                explicit Signature(const std::string & signature);

                const std::string & str() const;

            private:
                std::string _signature;
        };

        // an item of another signature than element_signature fails when written
        class Array
        {
            public:
                Array(const std::string & element_signature, std::vector<Value> items);

                const std::string & element_signature() const;
                const std::vector<Value> & items() const;

            private:
                std::string _element_signature;
                std::vector<Value> _items;
        };

        class DictEntry
        {
            public:
                DictEntry(Value key, Value value);

                const Value & key() const;
                const Value & value() const;

            private:
                std::vector<Value> _key_value;
        };

        class Struct
        {
            public:
                explicit Struct(std::initializer_list<Value> fields);

                const std::vector<Value> & fields() const;

            private:
                std::vector<Value> _fields;
        };

        class Variant
        {
            public:
                explicit Variant(Value value);

                const Value & value() const;

            private:
                // one item: a vector holds the recursive type
                std::vector<Value> _value;
        };

        // a{sv}: every value goes in a variant
        using Dict = std::vector<std::pair<std::string, Value>>;
        using Data = std::variant<bool,
                                  uint8_t,
                                  int16_t,
                                  uint16_t,
                                  int32_t,
                                  uint32_t,
                                  int64_t,
                                  uint64_t,
                                  double,
                                  std::string,
                                  ObjectPath,
                                  Signature,
                                  Array,
                                  DictEntry,
                                  Struct,
                                  Variant>;

        template <FixedType T>
        Value(T value);
        Value(const char *value);
        Value(const std::string & value);
        Value(sihd::util::ZStringView value);
        Value(ObjectPath value);
        Value(Signature value);
        Value(Array value);
        Value(DictEntry value);
        Value(Struct value);
        Value(Variant value);
        Value(Dict entries);
        // {{"key", v}, ...} is an a{sv}, {} an empty one, any other list an array: Value {x} wraps x, nest Dict {...}
        Value(std::initializer_list<Value> items);

        template <typename T>
        Value(const std::vector<T> & items)
        {
            std::vector<Value> values;
            values.reserve(items.size());
            for (const T & item : items)
                values.emplace_back(item);
            _data = Array(signature_of<T>(), std::move(values));
        }

        template <BasicType Key, typename T>
        Value(const std::map<Key, T> & entries)
        {
            std::vector<Value> values;
            for (const auto & [key, value] : entries)
                values.emplace_back(DictEntry(key, value));
            _data = Array("{" + signature_of<Key>() + signature_of<T>() + "}", std::move(values));
        }

        // a{Kv}: each value goes in a variant
        template <BasicType Key, typename T>
        static Value variant_dict(const std::map<Key, T> & entries)
        {
            std::vector<Value> values;
            for (const auto & [key, value] : entries)
                values.emplace_back(DictEntry(key, Variant(value)));
            return Array("{" + signature_of<Key>() + "v}", std::move(values));
        }

        // an empty container needs it: a Value, a Struct or a std::map of Value has no signature before its content
        template <typename T>
        static std::string signature_of()
        {
            if constexpr (BasicType<T>)
                return std::string(1, dbus_type_of<T>());
            else if constexpr (std::same_as<T, ObjectPath>)
                return "o";
            else if constexpr (std::same_as<T, Signature>)
                return "g";
            else if constexpr (std::same_as<T, Variant>)
                return "v";
            else if constexpr (std::same_as<T, Dict>)
                return "a{sv}";
            else if constexpr (requires {
                                   typename T::value_type;
                                   requires std::same_as<T, std::vector<typename T::value_type>>;
                               })
                return "a" + signature_of<typename T::value_type>();
            else if constexpr (requires {
                                   typename T::key_type;
                                   typename T::mapped_type;
                                   requires std::same_as<T, std::map<typename T::key_type, typename T::mapped_type>>;
                               })
                return "a{" + signature_of<typename T::key_type>() + signature_of<typename T::mapped_type>() + "}";
            else
                static_assert(!std::same_as<T, T>, "no static dbus signature for this type");
        }

        std::string signature() const;
        const Data & data() const;

    private:
        Data _data;
};

using ObjectPath = Value::ObjectPath;
using Signature = Value::Signature;
using Struct = Value::Struct;
using Variant = Value::Variant;
using Dict = Value::Dict;

} // namespace sihd::sys::linux_::dbus

#endif
