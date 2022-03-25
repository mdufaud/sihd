#ifndef __SIHD_UTIL_VALUE_HPP__
# define __SIHD_UTIL_VALUE_HPP__

# include <sihd/util/Types.hpp>
# include <sihd/util/Str.hpp>
# include <sihd/util/Logger.hpp>
# include <string.h>
# include <limits>

namespace sihd::util
{

SIHD_LOGGER;

class Value
{
    public:
        Value(): type(TYPE_NONE), data(0) {}

        Value(bool val) { this->set<bool>(val); }
        Value(char val) { this->set<char>(val); }
        Value(int8_t val) { this->set<int8_t>(val); }
        Value(uint8_t val) { this->set<uint8_t>(val); }
        Value(int16_t val) { this->set<int16_t>(val); }
        Value(uint16_t val) { this->set<uint16_t>(val); }
        Value(int32_t val) { this->set<int32_t>(val); }
        Value(uint32_t val) { this->set<uint32_t>(val); }
        Value(int64_t val) { this->set<int64_t>(val); }
        Value(uint64_t val) { this->set<uint64_t>(val); }
        Value(float val) { this->set<float>(val); }
        Value(double val) { this->set<double>(val); }

        Value(const uint8_t *buf, Type type) { this->set(buf, type); }

        virtual ~Value() {}

        template <typename T>
        static Value create(const T & val)
        {
            Value ret;
            ret.set<T>(val);
            return ret;
        }

        inline bool empty() const { return this->type == TYPE_NONE; }

        void clear()
        {
            this->type = TYPE_NONE;
            this->data = 0;
        }

        template <typename T>
        void set(T val)
        {
            static_assert(std::is_fundamental<T>::value);
            this->type = Types::to_type<T>();
            this->data = 0;
            memcpy(&this->data, &val, sizeof(T));
        }

        void set(const uint8_t *buf, Type type)
        {
            this->type = type;
            this->data = 0;
            memcpy(&this->data, buf, Types::type_size(type));
        }

        bool from_bool_string(const std::string & str)
        {
            bool val;
            bool ret = Str::convert_from_string<bool>(str, val);
            if (ret)
                this->set<bool>(val);
            return ret;
        }

        bool from_char_string(const std::string & str)
        {
            char val;
            bool ret = Str::convert_from_string<char>(str, val);
            if (ret)
                this->set<char>(val);
            return ret;
        }

        bool from_int_string(const std::string & str)
        {
            bool ret = false;
            if (Str::starts_with(str, "0b"))
                ret = Str::convert_from_string<int64_t>(str.c_str() + 2, this->data, 2);
            else
                ret = Str::convert_from_string<int64_t>(str, this->data);
            if (ret)
                this->type = TYPE_LONG;
            return ret;
        }

        bool from_float_string(const std::string & str)
        {
            if (str.find('.') == std::string::npos)
                return false;
            bool ret = false;
            if (Str::ends_with(str, "f"))
            {
                float f;
                ret = Str::convert_from_string<float>(str, f);
                if (ret)
                    this->set<float>(f);
            }
            else
            {
                double dbl;
                ret = Str::convert_from_string<double>(str, dbl);
                if (ret)
                    this->set<double>(dbl);
            }
            return ret;
        }

        bool from_any_string(const std::string & str)
        {
            return this->from_float_string(str)
                    || this->from_int_string(str)
                    || this->from_bool_string(str)
                    || this->from_char_string(str);
        }

        std::string to_string() const
        {
            if (this->type == TYPE_FLOAT)
                return std::to_string(this->to_float());
            else if (this->type == TYPE_DOUBLE)
                return std::to_string(this->to_double());
            return std::to_string(this->data);
        }

        inline bool is_float() const { return this->type == TYPE_FLOAT || this->type == TYPE_DOUBLE; }

        float to_float() const
        {
            float ret;
            memcpy(&ret, &this->data, sizeof(float));
            return ret;
        }

        double to_double() const
        {
            double ret;
            memcpy(&ret, &this->data, sizeof(double));
            return ret;
        }

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

        int compare_float(float cmp_val) const
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

        int compare_double(double cmp_val) const
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

        int compare_float_epsilon(float cmp_val, float epsilon) const
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

        int compare_double_epsilon(double cmp_val, double epsilon) const
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

}

#endif