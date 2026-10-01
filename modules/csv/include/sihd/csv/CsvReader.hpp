#ifndef __SIHD_CSV_CSVREADER_HPP__
#define __SIHD_CSV_CSVREADER_HPP__

#include <expected>
#include <string>
#include <string_view>

#include <sihd/sys/LineReader.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Splitter.hpp>

namespace sihd::csv
{

class CsvReader: public sihd::util::IReaderTimestamp
{
    public:
        CsvReader();
        CsvReader(std::string_view path);
        virtual ~CsvReader();

        std::expected<void, sihd::util::Error> set_delimiter(int c);
        std::expected<void, sihd::util::Error> set_commentary(int c);
        void set_timestamp_col(int n);
        void set_timestamp_format(std::string format);

        std::expected<void, sihd::util::Error> open(std::string_view path);
        bool is_open() const;
        bool close();

        std::expected<bool, sihd::util::Error> read_next() override;
        bool get_read_data(sihd::util::ArrCharView & view) const override;
        std::expected<sihd::util::Timestamp, sihd::util::Error> get_read_timestamp() const override;

        const std::vector<std::string> & columns() const;

    protected:

    private:
        void _reset_line();

        int _comment = '#';
        int _delimiter = ',';

        int _timestamp_col = -1;
        std::string _timestamp_fmt;

        bool _has_data;

        std::string _line;
        mutable std::vector<std::string> _csv_cols;

        sihd::util::Splitter _splitter;
        sihd::sys::LineReader _line_reader;
};

} // namespace sihd::csv

#endif