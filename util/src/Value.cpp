#include <sihd/util/Value.hpp>

namespace sihd::util
{

Value::Value(): type(TYPE_NONE), data(0) {}

Value::Value(const uint8_t *buf, Type type)
{
    this->set(buf, type);
}

bool Value::empty() const
{
    return this->type == TYPE_NONE;
}

void Value::clear()
{
    this->type = TYPE_NONE;
    this->data = 0;
}

void Value::set(const uint8_t *buf, Type type)
{
    this->type = type;
    this->data = 0;
    memcpy(&this->data, buf, Types::type_size(type));
}

bool Value::from_bool_string(const std::string & str)
{
    bool val;
    bool ret = str::convert_from_string<bool>(str, val);
    if (ret)
        this->set<bool>(val);
    return ret;
}

bool Value::from_char_string(const std::string & str)
{
    char val;
    bool ret = str::convert_from_string<char>(str, val);
    if (ret)
        this->set<char>(val);
    return ret;
}

bool Value::from_int_string(const std::string & str)
{
    bool ret = false;
    if (str::starts_with(str, "0b"))
        ret = str::convert_from_string<int64_t>(str.c_str() + 2, this->data, 2);
    else
        ret = str::convert_from_string<int64_t>(str, this->data);
    if (ret)
        this->type = TYPE_LONG;
    return ret;
}

bool Value::from_float_string(const std::string & str)
{
    if (str.find('.') == std::string::npos)
        return false;
    bool ret = false;
    if (str::ends_with(str, "f"))
    {
        float f;
        ret = str::convert_from_string<float>(str, f);
        if (ret)
            this->set<float>(f);
    }
    else
    {
        double dbl;
        ret = str::convert_from_string<double>(str, dbl);
        if (ret)
            this->set<double>(dbl);
    }
    return ret;
}

bool Value::from_any_string(const std::string & str)
{
    return this->from_float_string(str) || this->from_int_string(str) || this->from_bool_string(str)
           || this->from_char_string(str);
}

std::string Value::str() const
{
    if (this->type == TYPE_FLOAT)
        return std::to_string(this->to_float());
    else if (this->type == TYPE_DOUBLE)
        return std::to_string(this->to_double());
    return std::to_string(this->data);
}

bool Value::is_float() const
{
    return this->type == TYPE_FLOAT || this->type == TYPE_DOUBLE;
}

float Value::to_float() const
{
    float ret;
    memcpy(&ret, &this->data, sizeof(float));
    return ret;
}

double Value::to_double() const
{
    double ret;
    memcpy(&ret, &this->data, sizeof(double));
    return ret;
}

int Value::compare_float(float cmp_val) const
{
    if (this->type == TYPE_FLOAT)
    {
        float epsilon = std::numeric_limits<float>::epsilon();
        float float_val = this->to_float();
        if (std::abs(float_val - cmp_val) < epsilon)
            return 0;
        return (float_val + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        float epsilon = std::numeric_limits<float>::epsilon();
        double double_val = this->to_double();
        double double_cmp_val = (double)cmp_val;
        if (std::abs(double_val - double_cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < double_cmp_val ? -1 : 1;
    }
    return this->compare<float>(cmp_val);
}

int Value::compare_double(double cmp_val) const
{
    if (this->type == TYPE_FLOAT)
    {
        float epsilon = std::numeric_limits<float>::epsilon();
        float float_val = this->to_float();
        double double_val = (double)float_val;
        if (std::abs(double_val - cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        double epsilon = std::numeric_limits<double>::epsilon();
        double double_val = this->to_double();
        if (std::abs(double_val - cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < cmp_val ? -1 : 1;
    }
    return this->compare<double>(cmp_val);
}

int Value::compare_float_epsilon(float cmp_val, float epsilon) const
{
    if (this->type == TYPE_FLOAT)
    {
        float float_val = this->to_float();
        if (std::abs(float_val - cmp_val) < epsilon)
            return 0;
        return (float_val + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        double double_val = this->to_double();
        double double_cmp_val = (double)cmp_val;
        if (std::abs(double_val - double_cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < double_cmp_val ? -1 : 1;
    }
    return this->compare<float>(cmp_val);
}

int Value::compare_double_epsilon(double cmp_val, double epsilon) const
{
    if (this->type == TYPE_FLOAT)
    {
        float float_val = this->to_float();
        double double_val = (double)float_val;
        if (std::abs(double_val - cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < cmp_val ? -1 : 1;
    }
    else if (this->type == TYPE_DOUBLE)
    {
        double double_val = this->to_double();
        if (std::abs(double_val - cmp_val) < epsilon)
            return 0;
        return (double_val + epsilon) < cmp_val ? -1 : 1;
    }
    return this->compare<double>(cmp_val);
}

template <>
int Value::compare(const Value & val) const
{
    if (val.type == TYPE_FLOAT)
    {
        return this->compare_float(val.to_float());
    }
    else if (val.type == TYPE_DOUBLE)
    {
        return this->compare_double(val.to_double());
    }
    return this->compare(val.data);
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
