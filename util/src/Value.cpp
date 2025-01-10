#include <sihd/util/Value.hpp>

namespace sihd::util
{

Value::Value(): type(TYPE_NONE), data({.n = 0}) {}

Value::Value(const uint8_t *buf, Type type)
{
    this->type = type;
    this->data.n = 0;
    memcpy(&this->data.n, buf, Types::type_size(type));
}

bool Value::empty() const
{
    return this->type == TYPE_NONE;
}

void Value::clear()
{
    this->type = TYPE_NONE;
    this->data.n = 0;
}

Value Value::from_bool_string(const std::string & str)
{
    bool val;
    if (str::convert_from_string<bool>(str, val))
        return Value(val);
    return {};
}

Value Value::from_char_string(const std::string & str)
{
    char val;
    if (str::convert_from_string<char>(str, val))
        return Value(val);
    return {};
}

Value Value::from_int_string(const std::string & str)
{
    constexpr uint16_t binary_base = 2;
    constexpr uint16_t decimal_base = 10;
    constexpr uint16_t hexa_base = 16;

    uint16_t base = decimal_base;
    uint8_t prefix_size = 0;
    bool is_signed = true;

    if (str::starts_with(str, "0b"))
    {
        prefix_size = 2;
        base = binary_base;
    }
    else if (str::starts_with(str, "0x"))
    {
        prefix_size = 2;
        base = hexa_base;
    }
    else if (str::starts_with(str, "u"))
    {
        is_signed = false;
        prefix_size = 1;
    }

    if (is_signed)
    {
        int64_t val;
        if (str::convert_from_string<int64_t>(str.c_str() + prefix_size, val, base))
            return Value(val);
    }
    else
    {
        uint64_t val;
        if (str::convert_from_string<uint64_t>(str.c_str() + prefix_size, val, base))
            return Value(val);
    }

    return {};
}

Value Value::from_float_string(const std::string & str)
{
    if (str.find('.') == std::string::npos)
        return {};
    if (str::ends_with(str, "f"))
    {
        float f;
        if (str::convert_from_string<float>(str, f))
            return Value(f);
    }
    else
    {
        double dbl;
        if (str::convert_from_string<double>(str, dbl))
            return Value(dbl);
    }
    return {};
}

Value Value::from_any_string(const std::string & str)
{
    Value ret;

    ret = Value::from_float_string(str);
    if (ret.empty())
        ret = Value::from_int_string(str);
    if (ret.empty())
        ret = Value::from_bool_string(str);
    if (ret.empty())
        ret = Value::from_char_string(str);
    return ret;
}

std::string Value::str() const
{
    if (this->type == TYPE_FLOAT)
        return std::to_string(this->data.f);
    else if (this->type == TYPE_DOUBLE)
        return std::to_string(this->data.d);
    return std::to_string(this->data.n);
}

bool Value::is_float() const
{
    return this->type == TYPE_FLOAT || this->type == TYPE_DOUBLE;
}

int Value::compare_float(float cmp_val) const
{
    if (this->type == TYPE_FLOAT)
    {
        float epsilon = std::numeric_limits<float>::epsilon();
        if (std::abs(this->data.f - cmp_val) < epsilon)
            return 0;
        return (this->data.f + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        double epsilon = std::numeric_limits<double>::epsilon();
        double double_cmp_val = (double)cmp_val;
        if (std::abs(this->data.d - double_cmp_val) < epsilon)
            return 0;
        return (this->data.d + epsilon) < double_cmp_val ? -1 : 1;
    }
    return this->compare<float>(cmp_val);
}

int Value::compare_double(double cmp_val) const
{
    if (this->type == TYPE_FLOAT)
    {
        float epsilon = std::numeric_limits<float>::epsilon();
        double double_val = (double)this->data.f;
        if (std::abs(double_val - cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        double epsilon = std::numeric_limits<double>::epsilon();
        if (std::abs(this->data.d - cmp_val) < epsilon)
            return 0;
        return (this->data.d + epsilon) < cmp_val ? -1 : 1;
    }
    return this->compare<double>(cmp_val);
}

int Value::compare_float_epsilon(float cmp_val, float epsilon) const
{
    if (this->type == TYPE_FLOAT)
    {
        if (std::abs(this->data.f - cmp_val) < epsilon)
            return 0;
        return (this->data.f + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        double double_cmp_val = (double)cmp_val;
        if (std::abs(this->data.d - double_cmp_val) < epsilon)
            return 0;
        return (this->data.d + epsilon) < double_cmp_val ? -1 : 1;
    }
    return this->compare<float>(cmp_val);
}

int Value::compare_double_epsilon(double cmp_val, double epsilon) const
{
    if (this->type == TYPE_FLOAT)
    {
        double double_val = (double)this->data.f;
        if (std::abs(double_val - cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        if (std::abs(this->data.d - cmp_val) < epsilon)
            return 0;
        return (this->data.d + epsilon) < cmp_val ? -1 : 1;
    }
    return this->compare<double>(cmp_val);
}

template <>
int Value::compare(const Value & val) const
{
    if (val.type == TYPE_FLOAT)
    {
        return this->compare_float(val.data.f);
    }
    else if (val.type == TYPE_DOUBLE)
    {
        return this->compare_double(val.data.d);
    }
    return this->compare(val.data.n);
}

template <>
bool Value::operator==(const float & val)
{
    return this->compare_float(val) == 0;
}

template <>
bool Value::operator!=(const float & val)
{
    return this->compare_float(val) != 0;
}

template <>
bool Value::operator>(const float & val)
{
    return this->compare_float(val) > 0;
}

template <>
bool Value::operator>=(const float & val)
{
    return this->compare_float(val) >= 0;
}

template <>
bool Value::operator<(const float & val)
{
    return this->compare_float(val) < 0;
}

template <>
bool Value::operator<=(const float & val)
{
    return this->compare_float(val) <= 0;
}

// double

template <>
bool Value::operator==(const double & val)
{
    return this->compare_double(val) == 0;
}

template <>
bool Value::operator!=(const double & val)
{
    return this->compare_double(val) != 0;
}

template <>
bool Value::operator>(const double & val)
{
    return this->compare_double(val) > 0;
}

template <>
bool Value::operator>=(const double & val)
{
    return this->compare_double(val) >= 0;
}

template <>
bool Value::operator<(const double & val)
{
    return this->compare_double(val) < 0;
}

template <>
bool Value::operator<=(const double & val)
{
    return this->compare_double(val) <= 0;
}

} // namespace sihd::util
