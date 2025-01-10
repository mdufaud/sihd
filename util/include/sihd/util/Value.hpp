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
            static_assert(std::is_fundamental<T>::value);
            this->type = Types::type<T>();
            this->data.n = val;
        }

        Value(float val)
        {
            this->type = Type::TYPE_FLOAT;
            this->data.f = val;
        }

        Value(double val)
        {
            this->type = Type::TYPE_DOUBLE;
            this->data.d = val;
        }

        Value(const uint8_t *buf, Type type);
        ~Value() = default;

        static Value from_bool_string(const std::string & str);
        static Value from_char_string(const std::string & str);
        static Value from_int_string(const std::string & str);
        static Value from_float_string(const std::string & str);
        static Value from_any_string(const std::string & str);

        bool empty() const;
        void clear();
        std::string str() const;
        bool is_float() const;

        template <typename T>
        int compare(const T & cmp_val) const
        {
            static_assert(std::is_fundamental<T>::value);
            int64_t casted_cmp_val = cmp_val;
            if (this->type == TYPE_FLOAT)
            {
                if (this->data.f == casted_cmp_val)
                    return 0;
                return this->data.f < casted_cmp_val ? -1 : 1;
            }
            else if (this->type == TYPE_DOUBLE)
            {
                if (this->data.d == casted_cmp_val)
                    return 0;
                return this->data.d < casted_cmp_val ? -1 : 1;
            }
            if (this->data.n == casted_cmp_val)
                return 0;
            return this->data.n < casted_cmp_val ? -1 : 1;
        }

        int compare_float(float cmp_val) const;
        int compare_double(double cmp_val) const;
        int compare_float_epsilon(float cmp_val, float epsilon) const;
        int compare_double_epsilon(double cmp_val, double epsilon) const;

        template <typename T>
        inline Value & operator=(const T & val)
        {
            *this = Value(val);
            return *this;
        }

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

        union PrimitiveTypeHolder
        {
                char c;
                int8_t b;
                uint8_t ub;
                int16_t s;
                uint16_t us;
                int32_t i;
                uint32_t ui;
                int64_t n;
                uint64_t un;
                float f;
                double d;
        };
        PrimitiveTypeHolder data;

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
