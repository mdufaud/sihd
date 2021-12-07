#ifndef __SIHD_UTIL_STR_HPP__
# define __SIHD_UTIL_STR_HPP__

# include <iostream>
# include <vector>
# include <string>
# include <sstream>
# include <mutex>
# include <map>
# include <optional>
# include <sihd/util/Num.hpp>

namespace sihd::util
{

class Str
{
    private:
        Str() {};
        ~Str() {};

        static const char g_escapes_open[];
        static const char g_escapes_close[];

        static const size_t g_buffer_size;
        static std::mutex g_buffer_mutex;
        static char g_buffer[];

    public:
        static size_t hexdump_cols;
        static char g_escape_char;

        static std::vector<std::string> split(const std::string & s, const std::string & delimiter);
        static std::vector<std::string> split(const std::string & s, const char *delimiter);

        static bool is_escape_sequence_open(int c);
        static bool is_escape_sequence_close(int c);
        /**
         * @brief check if a char is escaped by calculating an impair number of escape '\' before the char
         *  must give the beginning pointer of string and the index of the actual char
         * 
         *  example:
         * str = "\[hello";
         * is_escaped_char(str, 1) == true
         * str = "\\[hello";
         * is_escaped_char(str, 1) == true
         * is_escaped_char(str, 2) == false
         */
        static bool is_escaped_char(const char *str, int index);
        static int closing_escape_of(int sequence);
        // [hello] -> 7
        static int get_closing_escape_index(const char *s, int index, const char *authorized_open_escape_sequences = nullptr);
        static std::vector<std::string> split_escape(const std::string & s, const char *delimiter,
                                                        const char *authorized_open_escape_sequences = nullptr);

        // static std::string remove_escape_char(const std::string & str);
        // static std::string remove_escape_sequences(const std::string & str, const char *authorized_open_escape_sequences = nullptr);

        static std::string join(const std::vector<std::string> & join_lst, const std::string & join_with = "");
        static std::string demangle(const char *name);
        static std::string format(const char *format, ...);
        static std::string trim(const std::string & s);
        static std::string & to_upper(std::string & s);
        static std::string & to_lower(std::string & s);
        static std::string replace(const std::string & s, const std::string & from, const std::string & to);

        static std::string addr_to_string(void *addr, size_t padding = 0);
        static std::string num_to_string(size_t num, uint16_t base);
        static char num_to_char(size_t num);
        static std::string hexdump(const void *mem, size_t size, char delim = ' ');
        // formatted hexdump
        static std::string hexdump_fmt(const void *mem, size_t size);

        static bool is_digit(int c, uint16_t base = 10);
        static bool is_number(const std::string & s, uint16_t base = 10);
        static bool starts_with(const std::string & s, const std::string & start);
        static bool ends_with(const std::string & s, const std::string & end);

        static std::map<std::string, std::string> parse_configuration(const std::string & conf);

        static bool to_long(const std::string & str, long *ret, uint16_t base = 0);
        static bool to_ulong(const std::string & str, unsigned long *ret, uint16_t base = 0);
        static bool to_double(const std::string & str, double *ret);

        template <typename T>
        static bool convert_from_string(const std::string & str, T & value, uint16_t base = 0);
};

template <>
bool Str::convert_from_string<bool>(const std::string & str, bool & value, uint16_t base);

template <>
bool Str::convert_from_string<char>(const std::string & str, char & value, uint16_t base);

template <>
bool Str::convert_from_string<int8_t>(const std::string & str, int8_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint8_t>(const std::string & str, uint8_t & value, uint16_t base);

template <>
bool Str::convert_from_string<int16_t>(const std::string & str, int16_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint16_t>(const std::string & str, uint16_t & value, uint16_t base);

template <>
bool Str::convert_from_string<int32_t>(const std::string & str, int32_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint32_t>(const std::string & str, uint32_t & value, uint16_t base);

template <>
bool Str::convert_from_string<int64_t>(const std::string & str, int64_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint64_t>(const std::string & str, uint64_t & value, uint16_t base);

template <>
bool Str::convert_from_string<float>(const std::string & str, float & value, uint16_t base);

template <>
bool Str::convert_from_string<double>(const std::string & str, double & value, uint16_t base);

}

#endif 