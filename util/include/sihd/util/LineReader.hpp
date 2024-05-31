#ifndef __SIHD_UTIL_LINEREADER_HPP__
#define __SIHD_UTIL_LINEREADER_HPP__

#include <sihd/util/File.hpp>
#include <sihd/util/IReader.hpp>

namespace sihd::util
{

class LineReader: public sihd::util::IReader
{
    public:
        struct LineReaderOptions
        {
                size_t read_buffsize = 4096;
                size_t line_buffsize = 512;
                bool delimiter_in_line = false;
                int delimiter = '\n';

                static LineReaderOptions none() { return LineReaderOptions {}; }
        };

        LineReader(const LineReaderOptions & options = LineReaderOptions::none());
        LineReader(std::string_view path, const LineReaderOptions & options = LineReaderOptions::none());
        LineReader(int fd, const LineReaderOptions & options = LineReaderOptions::none());
        LineReader(FILE *stream, bool ownership, const LineReaderOptions & options = LineReaderOptions::none());
        ~LineReader();

        static bool fast_read_line(std::string & line,
                                   FILE *stream = stdin,
                                   const LineReaderOptions & options = LineReaderOptions::none());

        static bool fast_read_stdin(std::string & line, LineReaderOptions options = LineReaderOptions::none());

        bool set_read_buffsize(size_t buffsize);
        bool set_line_buffsize(size_t buffsize);
        bool set_delimiter_in_line(bool active);
        bool set_delimiter(int c);

        bool open(std::string_view path);
        bool open_fd(int fd);
        bool set_stream(FILE *stream, bool ownership = false);
        bool is_open() const;
        bool close();

        bool read_next();
        bool get_read_data(ArrCharView & view) const;

        bool error() const { return _error; }
        size_t buffsize() const { return _read_buff_size; }
        const sihd::util::File & file() const { return _file; }
        size_t line_buffsize() const { return _line_buff_size; }

    protected:

    private:
        bool _init();
        bool _allocate_read_buffer();
        bool _allocate_line();
        bool _reallocate_line(size_t needed);
        void _reset();
        void _delete_buffers();

        sihd::util::File _file;

        size_t _read_buff_size;
        char *_read_ptr;

        size_t _line_buff_size;

        size_t _line_size;
        char *_line_ptr;

        size_t _last_read_index;
        ssize_t _read_size;
        bool _error;

        bool _put_delimiter_in_line;
        int _delimiter;
};

} // namespace sihd::util

#endif