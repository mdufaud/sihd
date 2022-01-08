#ifndef __SIHD_UTIL_LINEREADER_HPP__
# define __SIHD_UTIL_LINEREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/IReader.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/File.hpp>
# include <queue>

namespace sihd::util
{

class LineReader:   public sihd::util::Named,
                    public sihd::util::IReader,
                    public sihd::util::Configurable
{
    public:
        LineReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~LineReader();

        static bool fast_read_line(std::string & line, FILE *stream = stdin, size_t buffsize = 1);

        bool set_read_buffsize(size_t buff);
        bool set_line_buffsize(size_t buff);
        bool set_delimiter_in_line(bool active);
        bool set_delimiter(int c);

        bool open(const std::string & path);
        bool is_open() const;
        bool close();

        bool set_stream(FILE *stream, bool ownership = false);

        bool read_next();
        bool get_read_data(char **data, size_t *size) const;

        size_t buffsize() const { return _read_buff_size; }
        const sihd::util::File & file() const { return _file; }


    protected:

    private:
        bool _init();

        sihd::util::File _file;

        bool _allocate_read_buffer();
        bool _allocate_line();
        bool _reallocate_line();
        void _reset();
        void _delete_buffers();

        size_t _read_buff_size;
        char *_read_ptr;

        size_t _line_buff_size;
        size_t _line_buff_step;
        char *_line_ptr;

        size_t _last_read_index;
        ssize_t _read_size;

        bool _put_delimiter_in_line;
        int _delimiter;
};

}

#endif