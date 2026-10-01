#ifndef __SIHD_SYS_FILE_HPP__
#define __SIHD_SYS_FILE_HPP__

#include <cstdio> // FILE
#include <expected>
#include <string>
#include <string_view>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/IArray.hpp>

namespace sihd::sys
{

class File
{
    public:
        File();
        File(int fd, std::string_view mode);
        File(FILE *stream, bool ownership);
        File(std::string_view path, std::string_view mode);
        File(File && other);
        ~File();

        // don't like hidden behavior - deleting copy operators
        File(const File & other) = delete;
        File & operator=(const File & other) = delete;

        File & operator=(File && other);

        operator int() const { return this->fd(); }
        operator bool() const { return this->is_open(); }
        operator FILE *() const { return _file_ptr; }

        // mode = r/w/a - r+/w+/a+ - rb/wb/ab - rb+/wb+/ab+
        std::expected<void, sihd::util::Error> open(std::string_view path, std::string_view mode);
        // mode = r/w/a - r+/w+/a+ - rb/wb/ab - rb+/wb+/ab+
        std::expected<void, sihd::util::Error> open_fd(int fd, std::string_view mode);
        std::expected<void, sihd::util::Error> set_stream(FILE *stream, bool ownership);
        std::expected<void, sihd::util::Error>
            open_tmp(std::string_view prefix, bool write_binary, std::string_view suffix = "");
        // open with setted buffer_size
        std::expected<void, sihd::util::Error> open_mem(std::string_view mode, std::string_view put_in_buffer = "");
        std::expected<void, sihd::util::Error> open_tmpfile();
        bool is_open() const;
        std::expected<void, sihd::util::Error> close();

        // if size == 0 -> buffer is null
        std::expected<void, sihd::util::Error> set_buffer_size(size_t size);
        // no buffering, immediately write change to disk
        void set_no_buffering();
        // full buffering (until flush)
        void set_buffering_line();
        // line buffering (flush when newline)
        void set_buffering_full();
        // apply buff mode to current stream
        std::expected<void, sihd::util::Error> buff_stream();

        // write changes to disk
        std::expected<void, sihd::util::Error> flush();
        std::expected<void, sihd::util::Error> flush_unlocked();

        // internal file descriptor
        int fd() const;
        int fd_unlocked() const;

        // error checking
        bool eof() const;
        bool eof_unlocked() const;
        int error() const;
        int error_unlocked() const;
        void clear_errors();

        std::expected<size_t, sihd::util::Error> write(const void *data, size_t size);
        std::expected<size_t, sihd::util::Error> write(sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> write_char(int c);

        std::expected<size_t, sihd::util::Error> write_unlocked(const void *data, size_t size);
        std::expected<size_t, sihd::util::Error> write_unlocked(sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> write_char_unlocked(int c);

        // 0 means end of file
        std::expected<size_t, sihd::util::Error> read(void *buf, size_t size);
        // read to string, because of short string optimization we can't use capacity()
        std::expected<size_t, sihd::util::Error> read(std::string & str, size_t size);
        // read into Array using it's capacity()
        std::expected<size_t, sihd::util::Error> read(sihd::util::IArray & array);

        // 0 means end of file
        std::expected<size_t, sihd::util::Error> read_line(char **line, size_t *size);
        std::expected<size_t, sihd::util::Error> read_line_delim(char **line, size_t *size, int delim);

        std::expected<void, sihd::util::Error> seek(long offset);
        std::expected<void, sihd::util::Error> seek_begin(long offset);
        std::expected<void, sihd::util::Error> seek_end(long offset);
        std::expected<long, sihd::util::Error> tell();
        std::expected<long, sihd::util::Error> file_size();

        void lock();
        bool trylock();
        void unlock();

        FILE *file() { return _file_ptr; }
        const std::string & path() const { return _path; }

        const char *buf() const { return _buf_ptr; }
        size_t buf_size() const { return _buf_size; }
        bool buffering_line() const { return _buf_mode == _IOLBF; }
        bool buffering_full() const { return _buf_mode == _IOFBF; }
        bool buffering_none() const { return _buf_mode == _IONBF; }

    protected:

    private:
        std::expected<void, sihd::util::Error> _seek(long offset, int origin);
        void _delete_buffer();
        bool _allocate_buffer_if_not_exists();

        FILE *_file_ptr;
        std::string _path;
        char *_buf_ptr;
        size_t _buf_size;
        int _buf_mode;
        bool _stream_ownership;
};

} // namespace sihd::sys

#endif
