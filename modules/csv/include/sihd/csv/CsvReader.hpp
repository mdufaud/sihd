#ifndef __SIHD_CSV_CSVREADER_HPP__
#define __SIHD_CSV_CSVREADER_HPP__

#include <string>

#include <sihd/sys/LineReader.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Splitter.hpp>

namespace sihd::csv
{

class CsvReader: public sihd::util::IReaderTimestamp
{
    public:
        CsvReader();
        CsvReader(const std::string & path);
        virtual ~CsvReader();

        sihd::util::Result<void> set_delimiter(int c);
        sihd::util::Result<void> set_commentary(int c);
        void set_timestamp_col(int n);
        void set_timestamp_format(std::string format);

        sihd::util::Result<void> open(const std::string & path);
        bool is_open() const;
        bool close();

        sihd::util::Result<bool> read_next() override;
        bool get_read_data(sihd::util::ArrCharView & view) const override;
        sihd::util::Result<sihd::util::Timestamp> get_read_timestamp() const override;

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