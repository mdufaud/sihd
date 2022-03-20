#include <sihd/util/Value.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

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
bool Value::operator==(const float & val) { return this->compare_float(val) == 0; }

template <>
bool Value::operator!=(const float & val) { return this->compare_float(val) != 0; }

template <>
bool Value::operator>(const float & val) { return this->compare_float(val) > 0; }

template <>
bool Value::operator>=(const float & val) { return this->compare_float(val) >= 0; }

template <>
bool Value::operator<(const float & val) { return this->compare_float(val) < 0; }

template <>
bool Value::operator<=(const float & val) { return this->compare_float(val) <= 0; }

// double

template <>
bool Value::operator==(const double & val) { return this->compare_double(val) == 0; }

template <>
bool Value::operator!=(const double & val) { return this->compare_double(val) != 0; }

template <>
bool Value::operator>(const double & val) { return this->compare_double(val) > 0; }

template <>
bool Value::operator>=(const double & val) { return this->compare_double(val) >= 0; }

template <>
bool Value::operator<(const double & val) { return this->compare_double(val) < 0; }

template <>
bool Value::operator<=(const double & val) { return this->compare_double(val) <= 0; }

}