#ifndef __SIHD_UTIL_VALUE_HPP__
#define __SIHD_UTIL_VALUE_HPP__

#include <type_traits>

#include <sihd/util/str.hpp>
#include <sihd/util/type.hpp>

namespace sihd::util
{

class Value
{
    public:
        Value();

        template <typename T>
        Value(T val)
        {
            constexpr Type value_type = type::from<T>();
            static_assert(value_type != TYPE_OBJECT, "Value does not support this type");
            this->type = value_type;
            this->data.n = val;
        }

        Value(float val);
        Value(double val);

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
            static_assert(type::from<T>() != TYPE_OBJECT, "Value does not support this type");

            if constexpr (std::is_floating_point_v<T>)
                return this->order(this->to_double(), static_cast<double>(cmp_val));
            else if (this->is_float())
                return this->order(this->to_double(), static_cast<double>(cmp_val));
            else if (type::is_unsigned(this->type))
            {
                if constexpr (std::is_unsigned_v<T>)
                    return this->order(this->data.un, static_cast<uint64_t>(cmp_val));
                else
                    return this->compare_mixed(this->data.un, static_cast<int64_t>(cmp_val));
            }
            else
            {
                if constexpr (std::is_signed_v<T>)
                    return this->order(this->data.n, static_cast<int64_t>(cmp_val));
                else
                    return this->compare_mixed(this->data.n, static_cast<uint64_t>(cmp_val));
            }
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
        inline bool operator==(const T & val) const
        {
            return this->compare<T>(val) == 0;
        }

        template <typename T>
        inline bool operator!=(const T & val) const
        {
            return this->compare<T>(val) != 0;
        }

        template <typename T>
        inline bool operator>(const T & val) const
        {
            return this->compare<T>(val) > 0;
        }

        template <typename T>
        inline bool operator>=(const T & val) const
        {
            return this->compare<T>(val) >= 0;
        }

        template <typename T>
        inline bool operator<(const T & val) const
        {
            return this->compare<T>(val) < 0;
        }

        template <typename T>
        inline bool operator<=(const T & val) const
        {
            return this->compare<T>(val) <= 0;
        }

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
        Type type;

    private:
        double to_double() const;

        static int compare_mixed(uint64_t uint_val, int64_t int_val);
        static int compare_mixed(int64_t int_val, uint64_t uint_val);

        template <typename T>
        static constexpr int order(const T & a, const T & b)
        {
            return a < b ? -1 : (a == b ? 0 : 1);
        }
};

template <>
int Value::compare(const Value & val) const;

// float

template <>
bool Value::operator==(const float & val) const;

template <>
bool Value::operator!=(const float & val) const;

template <>
bool Value::operator>(const float & val) const;

template <>
bool Value::operator>=(const float & val) const;

template <>
bool Value::operator<(const float & val) const;

template <>
bool Value::operator<=(const float & val) const;

// double

template <>
bool Value::operator==(const double & val) const;

template <>
bool Value::operator!=(const double & val) const;

template <>
bool Value::operator>(const double & val) const;

template <>
bool Value::operator>=(const double & val) const;

template <>
bool Value::operator<(const double & val) const;

template <>
bool Value::operator<=(const double & val) const;

} // namespace sihd::util

#endif
