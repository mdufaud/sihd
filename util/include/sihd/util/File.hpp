#ifndef __SIHD_UTIL_FILE_HPP__
#define __SIHD_UTIL_FILE_HPP__

#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/ArrayView.hpp>

struct stat;

namespace sihd::util
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
        bool open(std::string_view path, std::string_view mode);
        // mode = r/w/a - r+/w+/a+ - rb/wb/ab - rb+/wb+/ab+
        bool open_fd(int fd, std::string_view mode);
        bool set_stream(FILE *stream, bool ownership);
        bool open_tmp(std::string_view prefix, bool write_binary, std::string_view suffix = "");
        // open with setted buffer_size
        bool open_mem(std::string_view mode, std::string_view put_in_buffer = "");
        bool open_tmpfile();
        bool is_open() const;
        bool close();

        // if size == 0 -> buffer is null
        bool set_buffer_size(size_t size);
        // no buffering, immediately write change to disk
        void set_no_buffering();
        // full buffering (until flush)
        void set_buffering_line();
        // line buffering (flush when newline)
        void set_buffering_full();
        // apply buff mode to current stream
        bool buff_stream();

        // write changes to disk
        bool flush();
        bool flush_unlocked();

        // internal file descriptor
        int fd() const;
        int fd_unlocked() const;

        // error checking
        bool eof() const;
        bool eof_unlocked() const;
        int error() const;
        int error_unlocked() const;
        void clear_errors();

        ssize_t write(const char *str, size_t size);
        ssize_t write(ArrCharView view);
        bool write_char(int c);

        ssize_t write_unlocked(const char *str, size_t size);
        ssize_t write_unlocked(ArrCharView view);
        bool write_char_unlocked(int c);

        ssize_t read(char *buf, size_t size);
        ssize_t read(std::string & str);
        ssize_t read(IArray & array);

        ssize_t read_line(char **line, size_t *size);
        ssize_t read_line_delim(char **line, size_t *size, int delim);

        bool seek(long offset);
        bool seek_begin(long offset);
        bool seek_end(long offset);
        long tell();
        long file_size();

        void lock();
        bool trylock();
        void unlock();

        FILE *file() { return _file_ptr; }
        const std::string & path() const { return _path; }

        std::optional<struct stat> stat();

        const char *buf() const { return _buf_ptr; }
        size_t buf_size() const { return _buf_size; }
        bool buffering_line() const { return _buf_mode == _IOLBF; }
        bool buffering_full() const { return _buf_mode == _IOFBF; }
        bool buffering_none() const { return _buf_mode == _IONBF; }

    protected:

    private:
        bool _seek(long offset, int origin);
        void _delete_buffer();
        bool _allocate_buffer_if_not_exists();

        FILE *_file_ptr;
        std::string _path;
        char *_buf_ptr;
        size_t _buf_size;
        int _buf_mode;
        bool _stream_ownership;
};

} // namespace sihd::util

#endif
