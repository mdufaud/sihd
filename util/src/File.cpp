#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/os.hpp>

#include <string.h>
#include <errno.h>
#include <limits.h>

namespace sihd::util
{

SIHD_LOGGER;

File::File()
{
    _file_ptr = nullptr;
    _buf_ptr = nullptr;
    _buf_size = 4096;
    // default is new line buffering
    _buf_mode = _IOLBF;
    _stream_ownership = true;
}

File::File(int fd, std::string_view mode): File()
{
    this->open_fd(fd, mode);
}

File::File(FILE *stream, bool ownership): File()
{
    this->set_stream(stream, ownership);
}

File::File(std::string_view path, std::string_view mode): File()
{
    this->open(path, mode);
}

File::File(File && other)
{
    *this = std::move(other);
}

File::~File()
{
    this->close();
    this->_delete_buffer();
}

File &  File::operator=(File && other)
{
    _file_ptr = other._file_ptr;
    _path = std::move(other._path);
    _buf_ptr = other._buf_ptr;
    _buf_size = other._buf_size;
    _buf_mode = other._buf_mode;
    _stream_ownership = other._stream_ownership;

    other._file_ptr = nullptr;
    other._buf_ptr = nullptr;
    other._buf_size = 0;
    return *this;
}

std::optional<struct stat>  File::stat()
{
    if (_file_ptr != nullptr)
    {
        struct stat stat;
        if (os::fstat(this->fd(), &stat, true))
            return stat;
    }
    return std::nullopt;
}

bool    File::set_buffer_size(size_t size)
{
    if (_buf_size == size)
        return true;
    if (size == 0)
        return false;
    this->_delete_buffer();
    _buf_size = size;
    return this->_allocate_buffer_if_not_exists();
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
    if (_buf_ptr == nullptr)
    {
        _buf_ptr = new char[_buf_size];
        if (_buf_ptr == nullptr)
        {
            SIHD_LOG(error, "File: could not allocate buffer: {}", _buf_size);
        }
        else
        {
            _buf_ptr[0] = 0;
            _buf_ptr[_buf_size - 1] = 0;
        }
    }
    return _buf_ptr != nullptr;
}

bool    File::buff_stream()
{
    if (_file_ptr != nullptr)
    {
        int ret = setvbuf(_file_ptr, _buf_ptr, _buf_mode, _buf_size);
        if (ret < 0)
        {
            SIHD_LOG(error, "File: could not set stream buffer: {}", strerror(errno));
            return false;
        }
    }
    return true;
}

void    File::_delete_buffer()
{
    if (_buf_ptr != nullptr)
    {
        delete[] _buf_ptr;
        _buf_ptr = nullptr;
        _buf_size = 0;
    }
}

bool    File::open_fd(int fd, std::string_view mode)
{
    this->close();
    _file_ptr = fdopen(fd, mode.data());
    if (_file_ptr == nullptr)
    {
        SIHD_LOG(error, "File: {}: for file descriptor {}", strerror(errno), fd);
    }
    else
        _stream_ownership = true;
    return _file_ptr != nullptr && this->_allocate_buffer_if_not_exists();
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
        SIHD_LOG(error, "File: could not open temporary file: {}", strerror(errno));
    }
    else
        _stream_ownership = true;
    return _file_ptr != nullptr && this->_allocate_buffer_if_not_exists();
}

bool    File::open_tmp(std::string_view prefix, bool write_binary, std::string_view suffix)
{
    this->close();
    const size_t path_size = prefix.size() + 6 + suffix.size();
    if (path_size > PATH_MAX)
    {
        SIHD_LOG(error, "File: Path too long: {}", path_size);
        return false;
    }
    char path[path_size];
    strcpy(path, prefix.data());
    strcpy(path + prefix.size(), "XXXXXX");
    strcpy(path + prefix.size() + 6, suffix.data());
#if !defined(__SIHD_WINDOWS__)
    int fd = mkstemps(path, suffix.size());
#else
    int fd = _mktemp_s(path, suffix.size());
#endif
    if (fd < 0)
    {
        SIHD_LOG(error, "File: could not open temporary file: {}", strerror(errno));
        return false;
    }
    if (this->open_fd(fd, write_binary ? "wb" : "w"))
        _path = path;
    return this->is_open();
}

bool    File::open(std::string_view path, std::string_view mode)
{
    this->close();
    _file_ptr = fopen(path.data(), mode.data());
    if (_file_ptr == nullptr)
    {
        SIHD_LOG(error, "File: {}: {}", strerror(errno), path);
    }
    else
    {
        _path = path;
        _stream_ownership = true;
    }
    return _file_ptr != nullptr && this->_allocate_buffer_if_not_exists();
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
    return  _file_ptr == nullptr ? -1 : fileno(_file_ptr);
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
        SIHD_LOG(error, "File: could not flush file: {}", strerror(errno));
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
            SIHD_LOG(error, "File: could not close file: {}", strerror(errno));
            return false;
        }
        _file_ptr = nullptr;
        _path.clear();
    }
    return true;
}

long    File::filesize()
{
    long ret = -1;
    long current_offset = this->tell();
    if (current_offset >= 0)
    {
        if (this->seek_end(0))
        {
            ret = this->tell();
            this->seek(current_offset);
        }
    }
    return ret;
}

void    File::lock()
{
#if !defined(__SIHD_WINDOWS__)
    flockfile(_file_ptr);
#else
    _lock_file(_file_ptr);
#endif
}

bool    File::trylock()
{
#if !defined(__SIHD_WINDOWS__)
    return ftrylockfile(_file_ptr) == 0;
#else
    _lock_file(_file_ptr);
    return false;
#endif
}

void    File::unlock()
{
#if !defined(__SIHD_WINDOWS__)
    funlockfile(_file_ptr);
#else
    _unlock_file(_file_ptr);
#endif
}

long    File::tell()
{
    long ret = ftell(_file_ptr);
    if (ret < 0)
        SIHD_LOG(error, "File: tell: {}", strerror(errno));
    return ret;
}

bool    File::seek(long offset)
{
    return this->_seek(offset, SEEK_CUR);
}

bool    File::seek_begin(long offset)
{
    return this->_seek(offset, SEEK_SET);
}

bool    File::seek_end(long offset)
{
    return this->_seek(offset, SEEK_END);
}

bool    File::_seek(long offset, int origin)
{
    int ret = fseek(_file_ptr, offset, origin);
    if (ret < 0)
        SIHD_LOG(error, "File: seek: {}", strerror(errno));
    return ret == 0;
}

ssize_t File::read(char *buf, size_t size)
{
    if (this->eof())
        return 0;
    size_t ret = fread(buf, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return (ssize_t)ret;
}

ssize_t File::read(IArray & array)
{
    ssize_t ret = this->read(reinterpret_cast<char *>(array.buf()), array.byte_capacity());
    if (ret >= 0)
        array.byte_resize((size_t)ret);
    return ret;
}

ssize_t File::write(const char *str, size_t size)
{
    size_t ret = fwrite(str, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return ret;
}

ssize_t File::write(std::string_view view)
{
    return this->write(view.data(), view.size());
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
