#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <string.h>
#include <errno.h>

namespace sihd::util
{

LOGGER;

File::File()
{
    this->_init();
}

File::File(int fd, const char *mode)
{
    this->_init();
    this->open_fd(fd, mode);
}

File::File(FILE *stream, bool ownership)
{
    this->_init();
    this->set_stream(stream, ownership);
}

File::File(const std::string & path, const char *mode)
{
    this->_init();
    this->open(path, mode);
}

File::~File()
{
    this->close();
    this->_delete_buffer();
}

void    File::_init()
{
    _file_ptr = nullptr;
    _buf = nullptr;
    _buf_size = 0;
    // default is new line buffering
    _buf_mode = _IOLBF;
    _stream_ownership = true;
}

std::optional<struct stat>  File::stat()
{
    if (_file_ptr != nullptr)
    {
        struct stat stat;
        if (OS::fstat(this->fd(), &stat, true))
            return stat;
    }
    return std::nullopt;
}

bool    File::set_bufsize(size_t size)
{
    if (_buf_size == size)
        return true;
    this->_delete_buffer();
    _buf_size = size;
    return size == 0 || this->_allocate_buffer_if_not_exists();
}

void    File::set_no_buffering()
{
    _buf_mode = _IONBF;
}

void    File::set_buffering_line()
{
    _buf_mode = _IOLBF;
}

void    File::set_buffering_full()
{
    _buf_mode = _IOFBF;
}

bool    File::_allocate_buffer_if_not_exists()
{
    if (_buf == nullptr)
        _buf = new char[_buf_size];
    return _buf != nullptr;
}

bool    File::buff_stream()
{
    if (_file_ptr != nullptr)
    {
        int ret = setvbuf(_file_ptr, _buf, _buf_mode, _buf_size);
        if (ret < 0)
        {
            LOG(error, "File: could not set stream buffer: " << strerror(errno));
            return false;
        }
    }
    return true;
}

void    File::_delete_buffer()
{
    if (_buf != nullptr)
    {
        delete[] _buf;
        _buf = nullptr;
        _buf_size = 0;
    }
}

bool    File::open_fd(int fd, const char *mode)
{
    this->close();
    _file_ptr = fdopen(fd, mode);
    if (_file_ptr == nullptr)
    {
        LOG(error, "File: could not open file descriptor: " << fd);
    }
    else
        _stream_ownership = true;
    return _file_ptr != nullptr;
}

bool    File::set_stream(FILE *stream, bool ownership)
{
    _file_ptr = stream;
    _stream_ownership = ownership;
    return true;
}

bool    File::open_tmpfile()
{
    this->close();
    _file_ptr = tmpfile();
    if (_file_ptr == nullptr)
    {
        LOG(error, "File: could not open temporary file");
    }
    else
        _stream_ownership = true;
    return _file_ptr != nullptr;
}

bool    File::open_tmp(const std::string & tmp_name_template, const char *mode)
{
    this->close();
    char path[tmp_name_template.size()];
    strcpy(path, tmp_name_template.c_str());
    size_t x_count = 0;
    size_t len = tmp_name_template.size();
    size_t i = 0;
    while (i < len)
    {
        x_count += (size_t)(path[i] == 'X');
        ++i;
    }
#if !defined(__SIHD_WINDOWS__)
    int fd = mkstemps(path, x_count);
#else
    int fd = _mktemp_s(path, x_count);
#endif
    if (fd < 0)
    {
        LOG(error, "File: could not open temporary file: " << strerror(errno));
        return false;
    }
    if (this->open_fd(fd, mode))
        _path = path;
    return this->is_open();
}

bool    File::open(const std::string & path, const char *mode)
{
    this->close();
    _file_ptr = fopen(path.c_str(), mode);
    if (_file_ptr == nullptr)
    {
        LOG(error, "File: could not open file: " << strerror(errno));
    }
    else
    {
        _path = path;
        _stream_ownership = true;
    }
    return _file_ptr != nullptr;
}

bool    File::is_open() const
{
    return _file_ptr != nullptr;
}

bool    File::eof() const
{
    return feof(_file_ptr) != 0;
}

int     File::fd() const
{
    return fileno(_file_ptr);
}

int     File::error() const
{
    return ferror(_file_ptr);
}

void    File::clear_errors()
{
    clearerr(_file_ptr);
}

bool    File::flush()
{
    if (_file_ptr != nullptr && fflush(_file_ptr) != 0)
    {
        LOG(error, "File: could not flush file: " << strerror(errno));
        return false;
    }
    return _file_ptr != nullptr;
}

bool    File::close()
{
    if (_file_ptr != nullptr)
    {
        if (_stream_ownership && fclose(_file_ptr) != 0)
        {
            LOG(error, "File: could not close file: " << strerror(errno));
            return false;
        }
        _file_ptr = nullptr;
        _path.clear();
    }
    return true;
}

long    File::tell()
{
    long ret = ftell(_file_ptr);
    if (ret < 0)
        LOG(error, "File: tell: " << strerror(errno));
    return ret;
}

int     File::seek(long offset)
{
    return this->_seek(offset, SEEK_CUR);
}

int     File::seek_begin(long offset)
{
    return this->_seek(offset, SEEK_SET);
}

int     File::seek_end(long offset)
{
    return this->_seek(offset, SEEK_END);
}

int     File::_seek(long offset, int origin)
{
    int ret = fseek(_file_ptr, offset, origin);
    if (ret < 0)
        LOG(error, "File: seek: " << strerror(errno));
    return ret;
}

ssize_t File::read(char *buf, size_t size)
{
    size_t ret = fread(buf, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return ret;
}

ssize_t File::write(const char *str, size_t size)
{
    size_t ret = fwrite(str, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return ret;
}

ssize_t File::write_str(const char *str)
{
    return fputs(str, _file_ptr);
}

ssize_t File::write_str(const std::string & str)
{
    return this->write_str(str.c_str());
}

bool    File::write_char(int c)
{
    return fputc(c, _file_ptr) == c;
}

ssize_t File::read_line(char **line, size_t *size)
{
#if !defined(__SIHD_WINDOWS__)
    return getline(line, size, _file_ptr);
#else
    *line = nullptr;
    *size = 0;
    return -1;
#endif
}

ssize_t File::read_line_delim(char **line, size_t *size, int delim)
{
#if !defined(__SIHD_WINDOWS__)
    return getdelim(line, size, delim, _file_ptr);
#else
    *line = nullptr;
    *size = 0;
    (void)delim;
    return -1;
#endif
}

}