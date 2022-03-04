#ifndef __SIHD_UTIL_VALUE_HPP__
# define __SIHD_UTIL_VALUE_HPP__

# include <sihd/util/Types.hpp>
# include <string.h>

namespace sihd::util
{

SIHD_LOGGER;

class Value
{
    public:
        Value() {}
        virtual ~Value() {}

        template <typename T>
        static Value create(T val)
        {
            Value ret;
            ret.set<T>(val);
            return ret;
        }

        template <typename T>
        void set(T val)
        {
            static_assert(sizeof(T) <= sizeof(this->data), "Value size needs to hold into a int64_t");
            this->type = Types::to_type<T>();
            this->data = 0;
            memcpy(&this->data, &val, sizeof(T));
        }

        template <typename T>
        int compare(const T & val) const
        {
            static_assert(sizeof(T) <= sizeof(this->data), "Value size needs to hold into a int64_t");
            return memcmp(&this->data, &val, sizeof(val));
        }

        int compare_float(double b, double epsilon = std::numeric_limits<double>::epsilon()) const
        {
            SIHD_TRACE("COMPARE FLOAT " << b);
            double a;
            memcpy(&a, &this->data, sizeof(double));
            if (std::abs(a - b) < epsilon)
                return 0;
            return (a + epsilon) < b ? -1 : 1;
        }

        template <typename T>
        inline bool operator==(const T & val) { return this->compare<T>(val) == 0; }

        template <typename T>
        inline bool operator!=(const T & val) { return this->compare<T>(val) != 0; }

        template <typename T>
        inline bool operator>(const T & val) { return this->compare<T>(val) > 0; }

        template <typename T>
        inline bool operator>=(const T & val) { return this->compare<T>(val) >= 0; }

        template <typename T>
        inline bool operator<(const T & val) { return this->compare<T>(val) < 0; }

        template <typename T>
        inline bool operator<=(const T & val) { return this->compare<T>(val) <= 0; }

        Type type;
        int64_t data;

    protected:

    private:
};

// float

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
bool Value::operator==(const double & val) { return this->compare_float(val) == 0; }

template <>
bool Value::operator!=(const double & val) { return this->compare_float(val) != 0; }

template <>
bool Value::operator>(const double & val) { return this->compare_float(val) > 0; }

template <>
bool Value::operator>=(const double & val) { return this->compare_float(val) >= 0; }

template <>
bool Value::operator<(const double & val) { return this->compare_float(val) < 0; }

template <>
bool Value::operator<=(const double & val) { return this->compare_float(val) <= 0; }



}

#endif