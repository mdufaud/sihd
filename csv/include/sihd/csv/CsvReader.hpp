#ifndef __SIHD_CSV_CSVREADER_HPP__
# define __SIHD_CSV_CSVREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/File.hpp>
# include <sihd/util/Configurable.hpp>

namespace sihd::csv
{

class CsvReader: public sihd::util::Named, public sihd::util::Configurable
{
    public:
        CsvReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvReader();

        bool set_delimiter(int c);
        bool set_commentary(int c);

        bool open(const std::string & path);
        bool is_open() const;
        bool close();

        bool read_next();
        bool get_values(std::vector<std::string> & data);

        int delimiter() const { return _delimiter; }
        int comment() const { return _comment; }

        const sihd::util::File & file() { return _file; };

    protected:

    private:
        void _free_line();

        int _delimiter;
        int _comment;
        char *_line_ptr;
        size_t _line_size;
        sihd::util::File _file;
};

}

#endif