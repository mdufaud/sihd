#ifndef __SIHD_CSV_CSVWRITER_HPP__
#define __SIHD_CSV_CSVWRITER_HPP__

#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/File.hpp>
#include <sihd/util/IWriter.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::csv
{

class CsvWriter: public sihd::util::IWriter
{
    public:
        CsvWriter();
        CsvWriter(const std::string & path, bool append = false);
        virtual ~CsvWriter();

        sihd::util::Result<void> set_delimiter(int c);
        sihd::util::Result<void> set_commentary(int c);

        sihd::util::Result<void> open(const std::string & path, bool append = false);
        bool is_open() const;
        bool close();

        ssize_t write(sihd::util::ArrCharView view) override;
        ssize_t write_row(sihd::util::ArrCharView view);
        ssize_t write(const std::vector<std::string> & values);
        ssize_t write_row(const std::vector<std::string> & values);
        ssize_t write_commentary(std::string_view commentary);

        sihd::util::Result<void> new_row();

        int delimiter() const;
        int comment() const;
        size_t current_row() const;
        size_t current_col() const;
        size_t max_col() const;

        const sihd::sys::File & file() const;

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