#ifndef __SIHD_UTIL_VALUE_HPP__
#define __SIHD_UTIL_VALUE_HPP__

#include <cstring>
#include <limits>

#include <sihd/util/Types.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

class Value
{
    public:
        Value();

        template <typename T>
        Value(T val)
        {
            this->set<T>(val);
        }

        Value(const uint8_t *buf, Type type);

        ~Value() = default;

        template <typename T>
        static Value create(const T & val)
        {
            Value ret;
            ret.set<T>(val);
            return ret;
        }

        bool empty() const;

        void clear();

        template <typename T>
        void set(T val)
        {
            static_assert(std::is_fundamental<T>::value);
            this->type = Types::type<T>();
            this->data = 0;
            memcpy(&this->data, &val, sizeof(T));
        }

        void set(const uint8_t *buf, Type type);

        bool from_bool_string(const std::string & str);

        bool from_char_string(const std::string & str);

        bool from_int_string(const std::string & str);

        bool from_float_string(const std::string & str);

        bool from_any_string(const std::string & str);

        std::string str() const;

        bool is_float() const;

        float to_float() const;

        double to_double() const;

        template <typename T>
        int compare(const T & cmp_val) const
        {
            static_assert(std::is_fundamental<T>::value);
            int64_t casted_cmp_val = cmp_val;
            if (this->type == TYPE_FLOAT)
            {
                float float_val = this->to_float();
                if (float_val == casted_cmp_val)
                    return 0;
                return float_val < casted_cmp_val ? -1 : 1;
            }
            else if (this->type == TYPE_DOUBLE)
            {
                double double_val = this->to_double();
                if (double_val == casted_cmp_val)
                    return 0;
                return double_val < casted_cmp_val ? -1 : 1;
            }
            if (this->data == casted_cmp_val)
                return 0;
            return this->data < casted_cmp_val ? -1 : 1;
        }

        int compare_float(float cmp_val) const;
        int compare_double(double cmp_val) const;
        int compare_float_epsilon(float cmp_val, float epsilon) const;
        int compare_double_epsilon(double cmp_val, double epsilon) const;

        template <typename T>
        inline bool operator==(const T & val)
        {
            return this->compare<T>(val) == 0;
        }

        template <typename T>
        inline bool operator!=(const T & val)
        {
            return this->compare<T>(val) != 0;
        }

        template <typename T>
        inline bool operator>(const T & val)
        {
            return this->compare<T>(val) > 0;
        }

        template <typename T>
        inline bool operator>=(const T & val)
        {
            return this->compare<T>(val) >= 0;
        }

        template <typename T>
        inline bool operator<(const T & val)
        {
            return this->compare<T>(val) < 0;
        }

        template <typename T>
        inline bool operator<=(const T & val)
        {
            return this->compare<T>(val) <= 0;
        }

        Type type;
        int64_t data;

    protected:

    private:
};

template <>
int Value::compare(const Value & val) const;

// float

template <>
bool Value::operator==(const float & val);

template <>
bool Value::operator!=(const float & val);

template <>
bool Value::operator>(const float & val);

template <>
bool Value::operator>=(const float & val);

template <>
bool Value::operator<(const float & val);

template <>
bool Value::operator<=(const float & val);

// double

template <>
bool Value::operator==(const double & val);

template <>
bool Value::operator!=(const double & val);

template <>
bool Value::operator>(const double & val);

template <>
bool Value::operator>=(const double & val);

template <>
bool Value::operator<(const double & val);

template <>
bool Value::operator<=(const double & val);

} // namespace sihd::util

#endif
