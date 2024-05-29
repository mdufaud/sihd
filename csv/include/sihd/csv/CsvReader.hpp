#ifndef __SIHD_CSV_CSVREADER_HPP__
#define __SIHD_CSV_CSVREADER_HPP__

#include <sihd/util/Array.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Splitter.hpp>

namespace sihd::csv
{

class CsvReader: public sihd::util::IReaderTimestamp
{
    public:
        CsvReader();
        CsvReader(std::string_view path);
        virtual ~CsvReader();

        bool set_delimiter(int c);
        bool set_commentary(int c);
        void set_timestamp_col(size_t n);
        void set_timestamp_format(std::string format);

        bool open(std::string_view path);
        bool is_open() const;
        bool close();

        bool read_next();
        bool get_read_data(sihd::util::ArrCharView & view) const;
        bool get_read_timestamp(time_t *nano_timestamp) const;

        const std::vector<std::string> & columns() const;

    protected:

    private:
        void _reset_line();

        int _comment;
        int _delimiter;

        int _timestamp_col;
        std::string _timestamp_fmt;

        bool _has_data;

        std::string _line;
        mutable std::vector<std::string> _csv_cols;

        sihd::util::Splitter _splitter;
        sihd::util::LineReader _line_reader;
};

} // namespace sihd::csv

#endif