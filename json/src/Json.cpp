#include <sihd/json/Json.hpp>

namespace sihd::json
{

// constructors

Json::Json(): _value(nullptr), _discarded(false) {}

Json::Json(std::nullptr_t): _value(nullptr), _discarded(false) {}

Json::Json(bool val): _value(val), _discarded(false) {}

Json::Json(int32_t val): _value(static_cast<int64_t>(val)), _discarded(false) {}

Json::Json(uint32_t val): _value(static_cast<uint64_t>(val)), _discarded(false) {}

Json::Json(int64_t val): _value(val), _discarded(false) {}

Json::Json(uint64_t val): _value(val), _discarded(false) {}

Json::Json(float val): _value(static_cast<double>(val)), _discarded(false) {}

Json::Json(double val): _value(val), _discarded(false) {}

Json::Json(const char *val): _value(std::string(val)), _discarded(false) {}

Json::Json(std::string_view val): _value(std::string(val)), _discarded(false) {}

Json::Json(std::string val): _value(std::move(val)), _discarded(false) {}

Json::Json(Array && arr): _value(std::make_unique<Array>(std::move(arr))), _discarded(false) {}

Json::Json(Object && obj): _value(std::make_unique<Object>(std::move(obj))), _discarded(false) {}

Json::Json(std::initializer_list<Json> init): _discarded(false)
{
    bool is_object_like = true;
    for (const auto & elem : init)
    {
        if (!elem.is_array() || elem.size() != 2)
        {
            is_object_like = false;
            break;
        }
        const auto *arr_ptr = std::get_if<ArrayPtr>(&elem._value);
        if (arr_ptr == nullptr || (*arr_ptr) == nullptr || !(**arr_ptr)[0].is_string())
        {
            is_object_like = false;
            break;
        }
    }

    if (is_object_like && init.size() > 0)
    {
        auto obj = std::make_unique<Object>();
        for (const auto & elem : init)
        {
            const auto & arr = *std::get<ArrayPtr>(elem._value);
            obj->emplace_back(arr[0].get<std::string>(), arr[1]);
        }
        _value = std::move(obj);
    }
    else
    {
        auto arr = std::make_unique<Array>(init);
        _value = std::move(arr);
    }
}

Json::Json(DiscardedTag): _value(nullptr), _discarded(true) {}

// copy

Json::Json(const Json & other):
    _discarded(other._discarded),
    _dom_holder(other._dom_holder),
    _dom_element(other._dom_element)
{
    struct CopyVisitor
    {
            Value operator()(std::nullptr_t) const { return nullptr; }
            Value operator()(bool v) const { return v; }
            Value operator()(int64_t v) const { return v; }
            Value operator()(uint64_t v) const { return v; }
            Value operator()(double v) const { return v; }
            Value operator()(const std::string & v) const { return v; }
            Value operator()(const ArrayPtr & v) const
            {
                if (!v)
                    return ArrayPtr(nullptr);
                return std::make_unique<Array>(*v);
            }
            Value operator()(const ObjectPtr & v) const
            {
                if (!v)
                    return ObjectPtr(nullptr);
                return std::make_unique<Object>(*v);
            }
    };
    _value = std::visit(CopyVisitor {}, other._value);
}

Json::Json(Json && other) noexcept:
    _value(std::move(other._value)),
    _discarded(other._discarded),
    _dom_holder(std::move(other._dom_holder)),
    _dom_element(other._dom_element)
{
    other._value = nullptr;
    other._discarded = false;
}

Json & Json::operator=(const Json & other)
{
    if (this != &other)
    {
        Json tmp(other);
        *this = std::move(tmp);
    }
    return *this;
}

Json & Json::operator=(Json && other) noexcept
{
    if (this != &other)
    {
        _value = std::move(other._value);
        _discarded = other._discarded;
        _dom_holder = std::move(other._dom_holder);
        _dom_element = other._dom_element;
        other._value = nullptr;
        other._discarded = false;
    }
    return *this;
}

Json::~Json() = default;


bool Json::is_null() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::NULL_VALUE;
    return !_discarded && std::holds_alternative<std::nullptr_t>(_value);
}

bool Json::is_bool() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::BOOL;
    return !_discarded && std::holds_alternative<bool>(_value);
}

bool Json::is_number_integer() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::INT64;
    return !_discarded && std::holds_alternative<int64_t>(_value);
}

bool Json::is_number_unsigned() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::UINT64;
    return !_discarded && std::holds_alternative<uint64_t>(_value);
}

bool Json::is_number_float() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::DOUBLE;
    return !_discarded && std::holds_alternative<double>(_value);
}

bool Json::is_number() const
{
    return is_number_integer() || is_number_unsigned() || is_number_float();
}

bool Json::is_string() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::STRING;
    return !_discarded && std::holds_alternative<std::string>(_value);
}

bool Json::is_array() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::ARRAY;
    return !_discarded && std::holds_alternative<ArrayPtr>(_value) && std::get<ArrayPtr>(_value) != nullptr;
}

bool Json::is_object() const
{
    if (_dom_holder != nullptr)
        return _dom_element.type() == simdjson::dom::element_type::OBJECT;
    return !_discarded && std::holds_alternative<ObjectPtr>(_value) && std::get<ObjectPtr>(_value) != nullptr;
}

bool Json::is_discarded() const
{
    return _discarded;
}

// type() is not inlined

Json::Type Json::type() const
{
    if (_discarded)
        return Type::Discarded;
    if (_dom_holder != nullptr)
    {
        switch (_dom_element.type())
        {
            case simdjson::dom::element_type::NULL_VALUE:
                return Type::Null;
            case simdjson::dom::element_type::BOOL:
                return Type::Bool;
            case simdjson::dom::element_type::INT64:
                return Type::Int;
            case simdjson::dom::element_type::UINT64:
                return Type::Uint;
            case simdjson::dom::element_type::DOUBLE:
                return Type::Float;
            case simdjson::dom::element_type::STRING:
                return Type::String;
            case simdjson::dom::element_type::ARRAY:
                return Type::Array;
            case simdjson::dom::element_type::OBJECT:
                return Type::Object;
        }
        return Type::Null;
    }
    switch (_value.index())
    {
        case 0:
            return Type::Null;
        case 1:
            return Type::Bool;
        case 2:
            return Type::Int;
        case 3:
            return Type::Uint;
        case 4:
            return Type::Float;
        case 5:
            return Type::String;
        case 6:
            return std::get<ArrayPtr>(_value) ? Type::Array : Type::Null;
        case 7:
            return std::get<ObjectPtr>(_value) ? Type::Object : Type::Null;
        default:
            return Type::Null;
    }
}

// element access

Json Json::operator[](std::string_view key) const
{
    if (_dom_holder != nullptr)
    {
        if (_dom_element.type() != simdjson::dom::element_type::OBJECT)
            return Json();
        simdjson::dom::element found;
        if (simdjson::dom::object(_dom_element).at_key(key).get(found) != simdjson::SUCCESS)
            return Json();
        Json child;
        child._dom_holder = _dom_holder;
        child._dom_element = found;
        return child;
    }
    if (!is_object())
        return Json();
    for (const auto & [k, v] : *std::get<ObjectPtr>(_value))
    {
        if (k == key)
            return v;
    }
    return Json();
}

Json Json::operator[](size_t index) const
{
    if (_dom_holder != nullptr)
    {
        if (_dom_element.type() != simdjson::dom::element_type::ARRAY)
            return Json();
        size_t i = 0;
        for (simdjson::dom::element child : simdjson::dom::array(_dom_element))
        {
            if (i == index)
            {
                Json j;
                j._dom_holder = _dom_holder;
                j._dom_element = child;
                return j;
            }
            ++i;
        }
        return Json();
    }
    if (!is_array())
        return Json();
    const auto & arr = *std::get<ArrayPtr>(_value);
    if (index >= arr.size())
        return Json();
    return arr[index];
}

size_t Json::size() const
{
    if (_dom_holder != nullptr)
    {
        auto dom_type = _dom_element.type();
        if (dom_type == simdjson::dom::element_type::ARRAY)
            return simdjson::dom::array(_dom_element).size();
        if (dom_type == simdjson::dom::element_type::OBJECT)
            return simdjson::dom::object(_dom_element).size();
        return 0;
    }
    if (is_array())
        return std::get<ArrayPtr>(_value)->size();
    if (is_object())
        return std::get<ObjectPtr>(_value)->size();
    return 0;
}

bool Json::empty() const
{
    if (_dom_holder != nullptr)
    {
        auto dom_type = _dom_element.type();
        if (dom_type == simdjson::dom::element_type::NULL_VALUE)
            return true;
        if (dom_type == simdjson::dom::element_type::ARRAY)
            return simdjson::dom::array(_dom_element).size() == 0;
        if (dom_type == simdjson::dom::element_type::OBJECT)
            return simdjson::dom::object(_dom_element).size() == 0;
        return false;
    }
    if (is_null())
        return true;
    if (is_array())
        return std::get<ArrayPtr>(_value)->empty();
    if (is_object())
        return std::get<ObjectPtr>(_value)->empty();
    return false;
}

// get specializations — read from DOM directly when available

template <>
bool Json::get<bool>() const
{
    if (_dom_holder != nullptr && _dom_element.type() == simdjson::dom::element_type::BOOL)
        return bool(_dom_element);
    if (is_bool())
        return std::get<bool>(_value);
    throw std::runtime_error("Json: not a boolean");
}

template <>
int64_t Json::get<int64_t>() const
{
    if (_dom_holder != nullptr)
    {
        auto t = _dom_element.type();
        if (t == simdjson::dom::element_type::INT64)
            return int64_t(_dom_element);
        if (t == simdjson::dom::element_type::UINT64)
            return static_cast<int64_t>(uint64_t(_dom_element));
        if (t == simdjson::dom::element_type::DOUBLE)
            return static_cast<int64_t>(double(_dom_element));
        throw std::runtime_error("Json: not a number");
    }
    if (is_number_integer())
        return std::get<int64_t>(_value);
    if (is_number_unsigned())
        return static_cast<int64_t>(std::get<uint64_t>(_value));
    if (is_number_float())
        return static_cast<int64_t>(std::get<double>(_value));
    throw std::runtime_error("Json: not a number");
}

template <>
uint64_t Json::get<uint64_t>() const
{
    if (_dom_holder != nullptr)
    {
        auto t = _dom_element.type();
        if (t == simdjson::dom::element_type::UINT64)
            return uint64_t(_dom_element);
        if (t == simdjson::dom::element_type::INT64)
            return static_cast<uint64_t>(int64_t(_dom_element));
        if (t == simdjson::dom::element_type::DOUBLE)
            return static_cast<uint64_t>(double(_dom_element));
        throw std::runtime_error("Json: not a number");
    }
    if (is_number_unsigned())
        return std::get<uint64_t>(_value);
    if (is_number_integer())
        return static_cast<uint64_t>(std::get<int64_t>(_value));
    if (is_number_float())
        return static_cast<uint64_t>(std::get<double>(_value));
    throw std::runtime_error("Json: not a number");
}

template <>
int32_t Json::get<int32_t>() const
{
    return static_cast<int32_t>(get<int64_t>());
}

template <>
uint32_t Json::get<uint32_t>() const
{
    return static_cast<uint32_t>(get<uint64_t>());
}

template <>
int16_t Json::get<int16_t>() const
{
    return static_cast<int16_t>(get<int64_t>());
}

template <>
uint16_t Json::get<uint16_t>() const
{
    return static_cast<uint16_t>(get<uint64_t>());
}

template <>
int8_t Json::get<int8_t>() const
{
    return static_cast<int8_t>(get<int64_t>());
}

template <>
uint8_t Json::get<uint8_t>() const
{
    return static_cast<uint8_t>(get<uint64_t>());
}

template <>
float Json::get<float>() const
{
    if (_dom_holder != nullptr)
    {
        auto t = _dom_element.type();
        if (t == simdjson::dom::element_type::DOUBLE)
            return static_cast<float>(double(_dom_element));
        if (t == simdjson::dom::element_type::INT64)
            return static_cast<float>(int64_t(_dom_element));
        if (t == simdjson::dom::element_type::UINT64)
            return static_cast<float>(uint64_t(_dom_element));
        throw std::runtime_error("Json: not a number");
    }
    if (is_number_float())
        return static_cast<float>(std::get<double>(_value));
    if (is_number_integer())
        return static_cast<float>(std::get<int64_t>(_value));
    if (is_number_unsigned())
        return static_cast<float>(std::get<uint64_t>(_value));
    throw std::runtime_error("Json: not a number");
}

template <>
double Json::get<double>() const
{
    if (_dom_holder != nullptr)
    {
        auto t = _dom_element.type();
        if (t == simdjson::dom::element_type::DOUBLE)
            return double(_dom_element);
        if (t == simdjson::dom::element_type::INT64)
            return static_cast<double>(int64_t(_dom_element));
        if (t == simdjson::dom::element_type::UINT64)
            return static_cast<double>(uint64_t(_dom_element));
        throw std::runtime_error("Json: not a number");
    }
    if (is_number_float())
        return std::get<double>(_value);
    if (is_number_integer())
        return static_cast<double>(std::get<int64_t>(_value));
    if (is_number_unsigned())
        return static_cast<double>(std::get<uint64_t>(_value));
    throw std::runtime_error("Json: not a number");
}

template <>
std::string Json::get<std::string>() const
{
    if (_dom_holder != nullptr && _dom_element.type() == simdjson::dom::element_type::STRING)
        return std::string(std::string_view(_dom_element));
    if (is_string())
        return std::get<std::string>(_value);
    throw std::runtime_error("Json: not a string");
}

// parsing — store only the DOM element, no eager variant build

Json Json::parse(std::string_view str)
{
    return parse(str, true);
}

Json Json::parse(std::string_view str, bool allow_exceptions)
{
    return parse(str.data(), str.data() + str.size(), nullptr, allow_exceptions);
}

Json Json::parse(const char *begin, const char *end, void * /*callback*/, bool allow_exceptions)
{
    try
    {
        size_t len = static_cast<size_t>(end - begin);
        auto holder = std::make_shared<DomHolder>();
        holder->source = simdjson::padded_string(begin, len);
        simdjson::dom::element root = holder->parser.parse(holder->source);
        Json result;
        result._dom_holder = std::move(holder);
        result._dom_element = root;
        return result;
    }
    catch (const simdjson::simdjson_error & e)
    {
        if (allow_exceptions)
            throw std::runtime_error(std::string("Json parse error: ") + e.what());
        return Json(DiscardedTag {});
    }
}

// serialization — use simdjson directly when DOM-backed

std::string Json::dump(int indent) const
{
    if (_dom_holder != nullptr)
    {
        if (indent >= 0)
            return simdjson::prettify(_dom_element);
        return simdjson::minify(_dom_element);
    }
    simdjson::builder::string_builder sb;
    _dump_to_builder(sb, indent, 0);
    std::string_view view;
    if (sb.view().get(view))
        return {};
    return std::string(view);
}

void Json::_write_indent(simdjson::builder::string_builder & sb, int indent, int depth)
{
    if (indent < 0)
        return;
    sb.append('\n');
    for (int i = 0; i < indent * depth; ++i)
        sb.append(' ');
}

void Json::_dump_to_builder(simdjson::builder::string_builder & sb, int indent, int depth) const
{
    if (_dom_holder != nullptr)
    {
        std::string s = simdjson::minify(_dom_element);
        sb.append_raw(std::string_view(s));
        return;
    }

    if (_discarded)
    {
        sb.append_raw("<discarded>");
        return;
    }

    switch (_value.index())
    {
        case 0: // nullptr_t
            sb.append_null();
            break;
        case 1: // bool
            sb.append(std::get<bool>(_value));
            break;
        case 2: // int64_t
            sb.append(std::get<int64_t>(_value));
            break;
        case 3: // uint64_t
            sb.append(std::get<uint64_t>(_value));
            break;
        case 4: // double
            sb.append(std::get<double>(_value));
            break;
        case 5: // string
            sb.escape_and_append_with_quotes(std::string_view(std::get<std::string>(_value)));
            break;
        case 6: // ArrayPtr
        {
            const auto & ptr = std::get<ArrayPtr>(_value);
            if (!ptr)
            {
                sb.append_null();
                break;
            }
            const auto & arr = *ptr;
            sb.start_array();
            for (size_t i = 0; i < arr.size(); ++i)
            {
                if (i > 0)
                    sb.append_comma();
                _write_indent(sb, indent, depth + 1);
                arr[i]._dump_to_builder(sb, indent, depth + 1);
            }
            _write_indent(sb, indent, depth);
            sb.end_array();
            break;
        }
        case 7: // ObjectPtr
        {
            const auto & ptr = std::get<ObjectPtr>(_value);
            if (!ptr)
            {
                sb.append_null();
                break;
            }
            const auto & obj = *ptr;
            sb.start_object();
            for (size_t i = 0; i < obj.size(); ++i)
            {
                if (i > 0)
                    sb.append_comma();
                _write_indent(sb, indent, depth + 1);
                sb.escape_and_append_with_quotes(std::string_view(obj[i].first));
                sb.append_colon();
                if (indent >= 0)
                    sb.append(' ');
                obj[i].second._dump_to_builder(sb, indent, depth + 1);
            }
            _write_indent(sb, indent, depth);
            sb.end_object();
            break;
        }
    }
}

// iterator

Json::iterator::iterator(IterType type, size_t index, const void *container):
    _iter_type(type),
    _index(index),
    _container(container)
{
}

Json::iterator::iterator(simdjson::dom::array::iterator arr_it,
                         simdjson::dom::array::iterator arr_end,
                         const std::shared_ptr<DomHolder> & holder):
    _iter_type(IterType::Array),
    _dom_mode(true),
    _arr_it(arr_it)
{
    [[maybe_unused]] auto ignored_end = arr_end;
    _current._dom_holder = holder;
}

Json::iterator::iterator(simdjson::dom::object::iterator obj_it,
                         simdjson::dom::object::iterator obj_end,
                         const std::shared_ptr<DomHolder> & holder):
    _iter_type(IterType::Object),
    _dom_mode(true),
    _obj_it(obj_it)
{
    [[maybe_unused]] auto ignored_end = obj_end;
    _current._dom_holder = holder;
}

Json::iterator::iterator(EndTag, IterType type, simdjson::dom::array::iterator arr_end):
    _iter_type(type),
    _dom_mode(true),
    _arr_it(arr_end)
{
}

Json::iterator::iterator(EndTag, IterType type, simdjson::dom::object::iterator obj_end):
    _iter_type(type),
    _dom_mode(true),
    _obj_it(obj_end)
{
}

const std::string & Json::iterator::key() const
{
    if (_dom_mode)
    {
        auto field = *_obj_it;
        _current_key = std::string(field.key);
        return _current_key;
    }
    const auto *obj = static_cast<const Object *>(_container);
    return (*obj)[_index].first;
}

const Json & Json::iterator::value() const
{
    if (_dom_mode)
    {
        if (_iter_type == IterType::Object)
        {
            auto field = *_obj_it;
            _current._dom_element = field.value;
        }
        else
        {
            _current._dom_element = *_arr_it;
        }
        _current._value = nullptr;
        return _current;
    }
    if (_iter_type == IterType::Object)
    {
        const auto *obj = static_cast<const Object *>(_container);
        return (*obj)[_index].second;
    }
    const auto *arr = static_cast<const Array *>(_container);
    return (*arr)[_index];
}

const Json & Json::iterator::operator*() const
{
    return value();
}

const Json *Json::iterator::operator->() const
{
    return &value();
}

Json::iterator & Json::iterator::operator++()
{
    if (_dom_mode)
    {
        if (_iter_type == IterType::Array)
            ++_arr_it;
        else
            ++_obj_it;
        return *this;
    }
    ++_index;
    return *this;
}

Json::iterator Json::iterator::operator++(int)
{
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool Json::iterator::operator==(const iterator & other) const
{
    if (_dom_mode != other._dom_mode)
        return false;
    if (_dom_mode)
    {
        if (_iter_type == IterType::Array)
            return _arr_it == other._arr_it;
        return _obj_it == other._obj_it;
    }
    return _index == other._index && _container == other._container;
}

bool Json::iterator::operator!=(const iterator & other) const
{
    return !(*this == other);
}

Json::iterator Json::begin() const
{
    if (_dom_holder != nullptr)
    {
        auto dom_type = _dom_element.type();
        if (dom_type == simdjson::dom::element_type::ARRAY)
        {
            simdjson::dom::array arr(_dom_element);
            return iterator(arr.begin(), arr.end(), _dom_holder);
        }
        if (dom_type == simdjson::dom::element_type::OBJECT)
        {
            simdjson::dom::object obj(_dom_element);
            return iterator(obj.begin(), obj.end(), _dom_holder);
        }
        return iterator(iterator::IterType::Array, 0, nullptr);
    }
    if (is_array())
        return iterator(iterator::IterType::Array, 0, std::get<ArrayPtr>(_value).get());
    if (is_object())
        return iterator(iterator::IterType::Object, 0, std::get<ObjectPtr>(_value).get());
    return iterator(iterator::IterType::Array, 0, nullptr);
}

Json::iterator Json::end() const
{
    if (_dom_holder != nullptr)
    {
        auto dom_type = _dom_element.type();
        if (dom_type == simdjson::dom::element_type::ARRAY)
        {
            simdjson::dom::array arr(_dom_element);
            return iterator(iterator::EndTag {}, iterator::IterType::Array, arr.end());
        }
        if (dom_type == simdjson::dom::element_type::OBJECT)
        {
            simdjson::dom::object obj(_dom_element);
            return iterator(iterator::EndTag {}, iterator::IterType::Object, obj.end());
        }
        return iterator(iterator::IterType::Array, 0, nullptr);
    }
    if (is_array())
    {
        const auto & arr = *std::get<ArrayPtr>(_value);
        return iterator(iterator::IterType::Array, arr.size(), &arr);
    }
    if (is_object())
    {
        const auto & obj = *std::get<ObjectPtr>(_value);
        return iterator(iterator::IterType::Object, obj.size(), &obj);
    }
    return iterator(iterator::IterType::Array, 0, nullptr);
}

} // namespace sihd::json
