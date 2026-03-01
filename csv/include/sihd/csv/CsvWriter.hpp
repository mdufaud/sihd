#ifndef __SIHD_CSV_CSVWRITER_HPP__
#define __SIHD_CSV_CSVWRITER_HPP__

#include <vector>

#include <sihd/sys/File.hpp>
#include <sihd/util/IWriter.hpp>

namespace sihd::csv
{

class CsvWriter: public sihd::util::IWriter
{
    public:
        CsvWriter();
        CsvWriter(std::string_view path, bool append = false);
        virtual ~CsvWriter();

        bool set_delimiter(int c);
        bool set_commentary(int c);

        bool open(std::string_view path, bool append = false);
        bool is_open() const;
        bool close();

        ssize_t write(sihd::util::ArrCharView view);
        ssize_t write_row(sihd::util::ArrCharView view);
        ssize_t write(const std::vector<std::string> & values);
        ssize_t write_row(const std::vector<std::string> & values);
        ssize_t write_commentary(std::string_view commentary);

        bool new_row();

        int delimiter() const { return _delimiter; }
        int comment() const { return _comment; }
        size_t current_row() const { return _row; }
        size_t current_col() const { return _col; }
        size_t max_col() const { return _max_col; }

        const sihd::sys::File & file() { return _file; };

    protected:

    private:
        size_t _max_col;
        size_t _col;
        size_t _row;

        int _delimiter;
        int _comment;
        int _line_feed;

        sihd::sys::File _file;
};

} // namespace sihd::csv

#endif