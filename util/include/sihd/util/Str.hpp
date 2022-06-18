#ifndef __SIHD_UTIL_STR_HPP__
# define __SIHD_UTIL_STR_HPP__

# include <iostream>
# include <vector>
# include <string>
# include <string_view>
# include <sstream>
# include <mutex>
# include <map>
# include <optional>

# include <sihd/util/Container.hpp>
# include <sihd/util/Num.hpp>
# include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Str
{
    private:
        Str() {};
        ~Str() {};

        static const size_t g_buffer_size;
        static std::mutex g_buffer_mutex;
        static char g_buffer[];

        static std::string _timeoffset_to_string(time_t nano, bool total_parenthesis, bool nano_resolution, bool localtime);
        static std::string _format_time(time_t nano,std::string_view format, bool localtime);

    public:
        static const char g_escapes_open[];
        static const char g_escapes_close[];
        static char g_escape_char;

        static size_t hexdump_cols;

        static void append_sep(std::string & str, std::string_view append, std::string_view sep = ",");

        static std::string timeoffset_str(Timestamp t, bool total_parenthesis = false, bool nano_resolution = false);
        static std::string localtimeoffset_str(Timestamp t, bool total_parenthesis = false, bool nano_resolution = false);
        // fmt strftime -> "%Y-%m-%d %H:%M:%S"
        static std::string format_time(Timestamp t, std::string_view format);
        static std::string format_localtime(Timestamp t, std::string_view format);

        // si -> 1K = 1000B
        // iec -> 1K = 1024B
        static std::string bytes_str(ssize_t bytes, bool iec = true);

        template <typename T>
        static std::string container_str(const T & container, Container::disable_if_map<T> = 0)
        {
            static_assert(Container::IsIterable<T>::value);
            std::stringstream ss;
            ss << "[";
            for (auto it = container.begin(), first = it; it != container.end(); ++it)
            {
                if (it != first)
                    ss << ", ";
                ss << *it;
            }
            ss << "]";
            return ss.str();
        }

        template <typename T>
        static std::string container_str(const T & container, Container::enable_if_map<T> = 0)
        {
            std::stringstream ss;
            ss << "{";
            for (auto it = container.begin(), first = it; it != container.end(); ++it)
            {
                if (it != first)
                    ss << ", ";
                ss << it->first << ": " << it->second;
            }
            ss << "}";
            return ss.str();
        }

        static bool is_escape_sequence_open(int c, const char *authorized_open_escape_sequences = nullptr);
        static bool is_escape_sequence_close(int c, const char *authorized_close_escape_sequences = nullptr);
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
        static int closing_escape_index(const char *s, int index, const char *authorized_open_escape_sequences = nullptr);

        static std::string remove_escape_char(std::string_view str);
        static std::string remove_escape_sequences(std::string_view str, const char *authorized_open_escape_sequences = nullptr);

        // clone str from_idx to size (or all the remaining str)
        static char *csub(std::string_view str, size_t from_idx, ssize_t size = 0);
        static std::string join(const std::vector<std::string> & join_lst, std::string_view join_with = "");
        static std::string demangle(std::string_view name);
        static std::string format(std::string_view format, ...);
        static std::string trim(std::string_view s);
        static std::string & to_upper(std::string & s);
        static std::string & to_lower(std::string & s);
        static std::string replace(std::string_view s, std::string_view from, std::string_view to);

        static std::string to_hex(uint64_t n);
        static std::string to_dec(uint64_t n);
        static std::string to_oct(uint64_t n);
        static std::string addr_str(void *addr, size_t padding = 0);
        static std::string num_str(int64_t num, uint16_t base);
        static char num_to_char(size_t num);
        static std::string hexdump(const void *mem, size_t size, char delim = ' ');
        // formatted hexdump
        static std::string hexdump_fmt(const void *mem, size_t size);

        static bool is_digit(int c, uint16_t base = 10);
        static bool is_number(std::string_view s, uint16_t base = 10);
        static bool starts_with(std::string_view s, std::string_view start);
        static bool ends_with(std::string_view s, std::string_view end);

        static std::map<std::string, std::string> parse_configuration(std::string_view conf);

        static bool to_long(std::string_view str, long *ret, uint16_t base = 0);
        static bool to_ulong(std::string_view str, unsigned long *ret, uint16_t base = 0);
        static bool to_llong(std::string_view str, long long *ret, uint16_t base = 0);
        static bool to_ullong(std::string_view str, unsigned long long *ret, uint16_t base = 0);
        static bool to_double(std::string_view str, double *ret);

        template <typename T>
        static bool convert_from_string(std::string_view str, T & value, uint16_t base = 0);
};

template <>
bool Str::convert_from_string<bool>(std::string_view str, bool & value, uint16_t base);

template <>
bool Str::convert_from_string<char>(std::string_view str, char & value, uint16_t base);

template <>
bool Str::convert_from_string<int8_t>(std::string_view str, int8_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint8_t>(std::string_view str, uint8_t & value, uint16_t base);

template <>
bool Str::convert_from_string<int16_t>(std::string_view str, int16_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint16_t>(std::string_view str, uint16_t & value, uint16_t base);

template <>
bool Str::convert_from_string<int32_t>(std::string_view str, int32_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint32_t>(std::string_view str, uint32_t & value, uint16_t base);

template <>
bool Str::convert_from_string<int64_t>(std::string_view str, int64_t & value, uint16_t base);

template <>
bool Str::convert_from_string<uint64_t>(std::string_view str, uint64_t & value, uint16_t base);

template <>
bool Str::convert_from_string<float>(std::string_view str, float & value, uint16_t base);

template <>
bool Str::convert_from_string<double>(std::string_view str, double & value, uint16_t base);

}

#endif