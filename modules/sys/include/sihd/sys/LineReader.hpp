#ifndef __SIHD_SYS_LINEREADER_HPP__
#define __SIHD_SYS_LINEREADER_HPP__

#include <expected>

#include <sihd/sys/File.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/IReader.hpp>

namespace sihd::sys
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

        static std::expected<std::string, sihd::util::Error>
            fast_read_line(FILE *stream = stdin, const LineReaderOptions & options = LineReaderOptions::none());

        static std::expected<std::string, sihd::util::Error>
            fast_read_stdin(LineReaderOptions options = LineReaderOptions::none());

        bool set_read_buffsize(size_t buffsize);
        bool set_line_buffsize(size_t buffsize);
        bool set_delimiter_in_line(bool active);
        bool set_delimiter(int c);

        std::expected<void, sihd::util::Error> open(std::string_view path);
        std::expected<void, sihd::util::Error> open_fd(int fd);
        std::expected<void, sihd::util::Error> set_stream(FILE *stream, bool ownership = false);
        bool is_open() const;
        bool close();

        // false means end of read, an error means the read failed
        std::expected<bool, sihd::util::Error> read_next();
        bool get_read_data(sihd::util::ArrCharView & view) const;

        size_t buffsize() const { return _read_buff_size; }
        const sihd::sys::File & file() const { return _file; }
        size_t line_buffsize() const { return _line_buff_size; }

    protected:

    private:
        bool _init();
        bool _allocate_read_buffer();
        bool _allocate_line();
        bool _reallocate_line(size_t needed);
        void _reset();

        sihd::sys::File _file;

        size_t _read_buff_size;
        sihd::util::ArrChar _read_buff;

        size_t _line_buff_size;

        size_t _line_size;
        sihd::util::ArrChar _line_buff;

        size_t _last_read_index;
        ssize_t _read_size;

        bool _put_delimiter_in_line;
        int _delimiter;
};

} // namespace sihd::sys

#endif
