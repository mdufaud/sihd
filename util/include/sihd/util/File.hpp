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
        File(int fd, const char *mode);
        File(FILE *stream, bool ownership);
        File(const std::string & path, const char *mode);
        virtual ~File();

        virtual bool open(const std::string & path, const char *mode);
        virtual bool open_fd(int fd, const char *mode);
        virtual bool set_stream(FILE *stream, bool ownership);
        // template must contain XXX
        virtual bool open_tmp(const std::string & tmp_name_template, const char *mode);
        virtual bool open_tmpfile();
        virtual bool is_open() const;
        virtual bool close();

        // if size == 0 -> buffer is null
        bool set_bufsize(size_t size);
        // no buffering, immediately write change to disk
        void set_no_buffering();
        // full buffering (until flush)
        void set_buffering_line();
        // line buffering (flush when newline)
        void set_buffering_full();

        // set buffer to stream
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
        ssize_t write(const std::string & str, size_t size_limit = 0);
        bool write_char(int c);

        int seek(long offset);
        int seek_begin(long offset);
        int seek_end(long offset);
        long tell();

        FILE *file() { return _file_ptr; }
        const std::string & path() const { return _path; }

        std::optional<struct stat> stat();

        const char *buf() const { return _buf_ptr; }
        size_t buf_size() const { return _buf_size; }
        bool buffering_line() const { return _buf_mode == _IOLBF; }
        bool buffering_full() const { return _buf_mode == _IOFBF; }
        bool buffering_none() const { return _buf_mode == _IONBF; }

    protected:
        virtual void _init();

    private:
        int _seek(long offset, int origin);
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