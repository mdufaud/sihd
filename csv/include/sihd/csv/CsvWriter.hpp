#ifndef __SIHD_CSV_CSVWRITER_HPP__
# define __SIHD_CSV_CSVWRITER_HPP__

# include <sihd/util/Named.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IWriter.hpp>
# include <sihd/util/File.hpp>

namespace sihd::csv
{

class CsvWriter:    public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IWriter
{
    public:
        CsvWriter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvWriter();

        bool set_quote_value(int c);
        bool set_delimiter(int c);
        bool set_commentary(int c);

        bool open(std::string_view path, bool append = false);
        bool is_open() const;
        bool close();

        bool new_row();
        ssize_t write_commentary(std::string_view commentary);

        // IWriterTimestamp
		ssize_t write(const char *data, size_t size);
		ssize_t write(const char *data, size_t size, time_t nano_timestamp);

        ssize_t write(std::string_view value);
        ssize_t write(std::string_view value, time_t nano_timestamp);

        ssize_t write(const std::vector<std::string> & values);
        ssize_t write(const std::vector<std::string> & values, time_t nano_timestamp);

        ssize_t write_row(const std::vector<std::string> & values);
        ssize_t write_row(const std::vector<std::string> & values, time_t nano_timestamp);

        int delimiter() const { return _delimiter; }
        int comment() const { return _comment; }
        size_t current_row() const { return _row; }
        size_t current_col() const { return _col; }
        size_t max_col() const { return _max_col; }

        const sihd::util::File & file() { return _file; };

    protected:

    private:
        size_t _max_col;
        size_t _col;
        size_t _row;

        int _delimiter;
        int _comment;
        int _line_feed;

        char _begin_quote_c;
        char _end_quote_c;

        sihd::util::File _file;
};

}

#endif