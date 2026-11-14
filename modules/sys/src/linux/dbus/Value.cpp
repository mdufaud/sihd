#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <algorithm>

# include <sihd/sys/linux/dbus/Value.hpp>

# include "Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using namespace sihd::util;

namespace
{

struct SignatureOf
{
        template <FixedType T>
        std::string operator()(const T &) const
        {
            return std::string(1, dbus_type_of<T>());
        }

        std::string operator()(const std::string &) const { return "s"; }

        std::string operator()(const Value::ObjectPath &) const { return "o"; }

        std::string operator()(const Value::Signature &) const { return "g"; }

        std::string operator()(const Value::Array & array) const { return "a" + array.element_signature(); }

        std::string operator()(const Value::DictEntry & entry) const
        {
            return "{" + entry.key().signature() + entry.value().signature() + "}";
        }

        std::string operator()(const Value::Struct & value) const
        {
            std::string signature = "(";
            for (const Value & field : value.fields())
                signature += field.signature();
            return signature + ")";
        }

        std::string operator()(const Value::Variant &) const { return "v"; }
};

bool is_key_value(const Value & item)
{
    const auto *pair = std::get_if<Value::Array>(&item.data());
    return pair != nullptr && pair->items().size() == 2 && std::holds_alternative<std::string>(pair->items()[0].data());
}

Value::Array variant_entries(const Value::Dict & entries)
{
    std::vector<Value> values;
    values.reserve(entries.size());
    for (const auto & [key, value] : entries)
        values.emplace_back(Value::DictEntry(key, Value::Variant(value)));
    return Value::Array("{sv}", std::move(values));
}

} // namespace

Value::ObjectPath::ObjectPath(const std::string & path): _path(path) {}

const std::string & Value::ObjectPath::str() const
{
    return _path;
}

Value::Signature::Signature(const std::string & signature): _signature(signature) {}

const std::string & Value::Signature::str() const
{
    return _signature;
}

Value::Array::Array(const std::string & element_signature, std::vector<Value> items):
    _element_signature(element_signature),
    _items(std::move(items))
{
}

const std::string & Value::Array::element_signature() const
{
    return _element_signature;
}

const std::vector<Value> & Value::Array::items() const
{
    return _items;
}

Value::DictEntry::DictEntry(Value key, Value value): _key_value {std::move(key), std::move(value)} {}

const Value & Value::DictEntry::key() const
{
    return _key_value[0];
}

const Value & Value::DictEntry::value() const
{
    return _key_value[1];
}

Value::Struct::Struct(std::initializer_list<Value> fields): _fields(fields) {}

const std::vector<Value> & Value::Struct::fields() const
{
    return _fields;
}

Value::Variant::Variant(Value value): _value {std::move(value)} {}

const Value & Value::Variant::value() const
{
    return _value[0];
}

template <FixedType T>
Value::Value(T value): _data(std::in_place_type<T>, value)
{
}

Value::Value(const char *value): _data(std::in_place_type<std::string>, value) {}

Value::Value(const std::string & value): _data(std::in_place_type<std::string>, value) {}

Value::Value(ZStringView value): _data(std::in_place_type<std::string>, value.c_str()) {}

Value::Value(ObjectPath value): _data(std::move(value)) {}

Value::Value(Signature value): _data(std::move(value)) {}

Value::Value(Array value): _data(std::move(value)) {}

Value::Value(DictEntry value): _data(std::move(value)) {}

Value::Value(Struct value): _data(std::move(value)) {}

Value::Value(Variant value): _data(std::move(value)) {}

Value::Value(Dict entries): _data(variant_entries(entries)) {}

Value::Value(std::initializer_list<Value> items)
{
    if (!std::ranges::all_of(items, is_key_value))
    {
        _data = Array(items.begin()->signature(), std::vector<Value>(items));
        return;
    }
    Dict entries;
    entries.reserve(items.size());
    for (const Value & item : items)
    {
        const std::vector<Value> & pair = std::get<Array>(item.data()).items();
        entries.emplace_back(std::get<std::string>(pair[0].data()), pair[1]);
    }
    _data = variant_entries(entries);
}

std::string Value::signature() const
{
    return std::visit(SignatureOf {}, _data);
}

const Value::Data & Value::data() const
{
    return _data;
}

# define SIHD_DBUS_INSTANTIATE_FIXED(T) template Value::Value(T);
SIHD_DBUS_FOR_EACH_FIXED_TYPE(SIHD_DBUS_INSTANTIATE_FIXED)
# undef SIHD_DBUS_INSTANTIATE_FIXED

} // namespace sihd::sys::linux_::dbus

#endif
