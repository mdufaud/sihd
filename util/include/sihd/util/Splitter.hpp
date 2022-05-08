#ifndef __SIHD_UTIL_SPLITTER_HPP__
# define __SIHD_UTIL_SPLITTER_HPP__

#include <sihd/util/Str.hpp>

namespace sihd::util
{

class Splitter
{
    public:
        Splitter();
        Splitter(int delimiter, const std::string & authorized_open_escape_sequences = "");
        Splitter(const std::string & delimiter, const std::string & authorized_open_escape_sequences = "");
        Splitter(int (*fun)(int), const std::string & authorized_open_escape_sequences = "");
        virtual ~Splitter();

        void set_delimiter_method(int (*fun)(int));

        bool set_empty_delimitations(bool active);
        bool set_delimiter(std::string_view delimiter);
        bool set_delimiter_spaces();
        bool set_escape_sequences(std::string_view sequences);
        bool set_escape_sequences_all();

        std::vector<std::string> split(std::string_view str) const;
        std::vector<std::string_view> split_view(std::string_view str) const;

        // count number of tokens to return
        int count_tokens(const char *s) const;
        // return the next token and updates the index
        std::string_view next_token(const char *s, int *idx) const;

    protected:

    private:
        // return the size of the delimiter matching the string c or -1
        int _get_delimiter_offset(const char *c) const;

        // if two delimiters must make an empty token
        bool _empty_delimitations;
        // a delimiter string to split on
        std::string _delimiter;
        // a series of opening sequences like '(' that'll ignore split when between an opening and closing sequence
        std::string _authorized_open_escape_sequences;
        // a method to match delimiter - overrides delimiter
        int (*_compare_method)(int c);
};

}

#endif