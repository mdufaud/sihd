#ifndef __SIHD_CSV_CSVWRITER_HPP__
# define __SIHD_CSV_CSVWRITER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <cstdio>

namespace sihd::csv
{

class CsvWriter: public sihd::util::Named, public sihd::util::Configurable
{
    public:
        CsvWriter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvWriter();

        bool set_delimiter(int c);
        bool set_commentary(int c);

        bool open(const std::string & path, bool append = false);
        bool is_open() const;
        bool close();

        bool new_row();
        bool write_commentary(const std::string & commentary);
        bool write(const std::string & value);
        bool write(const std::vector<std::string> & values);
        bool write_row(const std::vector<std::string> & values);

        int delimiter() const { return _delimiter; }
        int comment() const { return _comment; }
        size_t current_row() const { return _row; }
        size_t current_col() const { return _col; }
        size_t max_col() const { return _max_col; }

    protected:

    private:
        size_t _max_col;
        size_t _col;
        size_t _row;
        int _delimiter;
        int _comment;
        int _line_feed;
        FILE *_file_ptr;
};

}

#endif