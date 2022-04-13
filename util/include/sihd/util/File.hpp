#ifndef __SIHD_UTIL_FILE_HPP__
# define __SIHD_UTIL_FILE_HPP__

# include <sihd/util/platform.hpp>
# include <sihd/util/OS.hpp>
# include <sihd/util/IArray.hpp>
# include <cstdio>
# include <string>
# include <optional>

namespace sihd::util
{

class File
{
    public:
        File();
        File(int fd, std::string_view mode);
        File(FILE *stream, bool ownership);
        File(std::string_view path, std::string_view mode);
        virtual ~File();

        virtual bool open(std::string_view path, std::string_view mode);
        virtual bool open_fd(int fd, std::string_view mode);
        virtual bool set_stream(FILE *stream, bool ownership);
        // template must contain XXX
        virtual bool open_tmp(std::string_view tmp_name_template, std::string_view mode);
        virtual bool open_tmpfile();
        virtual bool is_open() const;
        virtual bool close();

        // if size == 0 -> buffer is null
        bool set_buffer_size(size_t size);
        // no buffering, immediately write change to disk
        void set_no_buffering();
        // full buffering (until flush)
        void set_buffering_line();
        // line buffering (flush when newline)
        void set_buffering_full();

        // apply buff mode to stream
        bool buff_stream();
        // write changes to disk
        bool flush();

        // internal file descriptor
        int fd() const;

        // error checking
        virtual bool eof() const;
        virtual int error() const;
        virtual void clear_errors();

        virtual ssize_t write(const char *str, size_t size);
        virtual ssize_t read(char *buf, size_t size);

        ssize_t read(IArray & array);
        ssize_t read_line(char **line, size_t *size);
        ssize_t read_line_delim(char **line, size_t *size, int delim);

        ssize_t write(const IArray & array, size_t byte_offset = 0);
        // can set a limit to the string length
        ssize_t write(std::string_view str, size_t size_limit = 0);
        bool write_char(int c);

        bool seek(long offset);
        bool seek_begin(long offset);
        bool seek_end(long offset);
        long tell();
        long filesize();

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
        bool _set_buffer_to_stream();

        FILE *_file_ptr;
        std::string _path;
        char *_buf_ptr;
        size_t _buf_size;
        int _buf_mode;
        bool _stream_ownership;
};

}

#endif