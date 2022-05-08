#ifndef __SIHD_CSV_CSVREADER_HPP__
# define __SIHD_CSV_CSVREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/File.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IReader.hpp>
# include <sihd/util/Splitter.hpp>

namespace sihd::csv
{

class CsvReader:    public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IReaderTimestamp
{
    public:
        CsvReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvReader();

        bool set_quote_value(int c);
        bool set_delimiter(int c);
        bool set_commentary(int c);

        bool open(std::string_view path);
        bool is_open() const;
        bool close();

        bool read_next();
        bool get_read_data(std::vector<std::string> & data) const;
		bool get_read_data(char **data, size_t *size) const;
		bool read_timestamp(time_t *nano_timestamp) const;

        int comment() const { return _comment; }

        const sihd::util::File & file() const { return _file; };

    protected:

    private:
        void _free_line();

        int _comment;
        int _quote;

        char *_line_ptr;
        size_t _line_size;

        sihd::util::Splitter _splitter;
        sihd::util::File _file;
};

}

#endif