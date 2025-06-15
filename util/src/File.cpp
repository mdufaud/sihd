#include <sys/stat.h>

#include <cerrno>
#include <climits>
#include <cstring>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/platform.hpp>

#if defined(__SIHD_WINDOWS__) or defined(__SIHD_ANDROID__)
# define fwrite_unlocked fwrite
# define fputc_unlocked fputc
# define feof_unlocked feof
# define ferror_unlocked ferror
# define fileno_unlocked fileno
# define fflush_unlocked fflush
#endif

#if defined(__SIHD_WINDOWS__)
# include <fcntl.h>
# include <io.h>
# include <share.h>
# include <sys/stat.h>
# include <windows.h>
#endif

namespace sihd::util
{

SIHD_LOGGER;

File::File()
{
    _file_ptr = nullptr;
    _buf_ptr = nullptr;
    _buf_size = 0;
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

File & File::operator=(File && other)
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

bool File::set_buffer_size(size_t size)
{
    if (_buf_size == size)
        return true;
    if (size == 0)
        return false;
    this->_delete_buffer();
    _buf_size = size;
    return this->_allocate_buffer_if_not_exists();
}

void File::set_no_buffering()
{
    _buf_mode = _IONBF;
}

void File::set_buffering_line()
{
    _buf_mode = _IOLBF;
}

void File::set_buffering_full()
{
    _buf_mode = _IOFBF;
}

bool File::_allocate_buffer_if_not_exists()
{
    if (_buf_ptr == nullptr && _buf_size > 0)
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
    return _buf_size == 0 || _buf_ptr != nullptr;
}

bool File::buff_stream()
{
    this->_allocate_buffer_if_not_exists();
    if (_file_ptr != nullptr && _buf_ptr != nullptr)
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

void File::_delete_buffer()
{
    if (_buf_ptr != nullptr)
    {
        delete[] _buf_ptr;
        _buf_ptr = nullptr;
        _buf_size = 0;
    }
}

bool File::open_fd(int fd, std::string_view mode)
{
    if (this->close() == false)
        return false;
    _file_ptr = fdopen(fd, mode.data());
    if (_file_ptr == nullptr)
    {
        SIHD_LOG(error, "File: {}: for file descriptor {}", strerror(errno), fd);
    }
    else
        _stream_ownership = true;
    return _file_ptr != nullptr;
}

bool File::set_stream(FILE *stream, bool ownership)
{
    if (this->close() == false)
        return false;
    _file_ptr = stream;
    _stream_ownership = ownership;
    return true;
}

bool File::open_tmpfile()
{
    if (this->close() == false)
        return false;
    _file_ptr = tmpfile();
    if (_file_ptr == nullptr)
    {
        SIHD_LOG(error, "File: could not open temporary file: {}", strerror(errno));
    }
    else
        _stream_ownership = true;
    return _file_ptr != nullptr;
}

bool File::open_tmp(std::string_view prefix, bool write_binary, std::string_view suffix)
{
    if (this->close() == false)
        return false;
    const size_t path_size = prefix.size() + 6 + suffix.size();
    if (path_size >= PATH_MAX)
    {
        SIHD_LOG(error, "File: Path too long: {}", path_size);
        return false;
    }
    char path[PATH_MAX];
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

bool File::open(std::string_view path, std::string_view mode)
{
    if (this->close() == false)
        return false;
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
    return _file_ptr != nullptr;
}

bool File::is_open() const
{
    return _file_ptr != nullptr;
}

bool File::eof() const
{
    return feof(_file_ptr) != 0;
}

bool File::eof_unlocked() const
{
    return feof_unlocked(_file_ptr) != 0;
}

int File::fd() const
{
    return _file_ptr == nullptr ? -1 : fileno(_file_ptr);
}

int File::fd_unlocked() const
{
    return _file_ptr == nullptr ? -1 : fileno_unlocked(_file_ptr);
}

int File::error() const
{
    return ferror(_file_ptr);
}

int File::error_unlocked() const
{
    return ferror_unlocked(_file_ptr);
}

void File::clear_errors()
{
    clearerr(_file_ptr);
}

bool File::flush()
{
    if (_file_ptr != nullptr && fflush(_file_ptr) != 0)
    {
        SIHD_LOG(error, "File: could not flush file: {}", strerror(errno));
        return false;
    }
    return _file_ptr != nullptr;
}

bool File::flush_unlocked()
{
    if (_file_ptr != nullptr && fflush_unlocked(_file_ptr) != 0)
    {
        SIHD_LOG(error, "File: could not flush file: {}", strerror(errno));
        return false;
    }
    return _file_ptr != nullptr;
}

bool File::close()
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

long File::file_size()
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

void File::lock()
{
#if !defined(__SIHD_WINDOWS__)
    flockfile(_file_ptr);
#else
    _lock_file(_file_ptr);
#endif
}

bool File::trylock()
{
#if !defined(__SIHD_WINDOWS__)
    return ftrylockfile(_file_ptr) == 0;
#else
    _lock_file(_file_ptr);
    return false;
#endif
}

void File::unlock()
{
#if !defined(__SIHD_WINDOWS__)
    funlockfile(_file_ptr);
#else
    _unlock_file(_file_ptr);
#endif
}

bool File::open_mem(std::string_view mode, std::string_view put_in_buffer)
{
    if (this->_allocate_buffer_if_not_exists() == false)
        return false;

    if (put_in_buffer.empty() == false)
    {
        const size_t max_len = std::min(_buf_size, put_in_buffer.size());

        _buf_ptr[0] = 0;
        strncpy(_buf_ptr, put_in_buffer.data(), max_len);

        if (max_len < _buf_size)
            memset(_buf_ptr + max_len, 0, _buf_size - max_len);
    }
    else
    {
        memset(_buf_ptr, 0, _buf_size);
    }

    FILE *stream;
#if defined(__SIHD_WINDOWS__)
    // https://github.com/Arryboom/fmemopen_windows
    int fd;
    char tp[MAX_PATH - 13];
    char fn[MAX_PATH + 1];
    int *pfd = &fd;
    int retner = -1;
    char tfname[] = "MemTF_";
    if (!GetTempPathA(sizeof(tp), tp))
        return false;
    if (!GetTempFileNameA(tp, tfname, 0, fn))
        return false;
    retner = _sopen_s(pfd,
                      fn,
                      _O_CREAT | _O_SHORT_LIVED | _O_TEMPORARY | _O_RDWR | _O_BINARY | _O_NOINHERIT,
                      _SH_DENYRW,
                      _S_IREAD | _S_IWRITE);
    if (retner != 0)
        return false;
    if (fd == -1)
        return false;

    if (_write(fd, _buf_ptr, _buf_size) != (ssize_t)_buf_size)
    {
        SIHD_LOG(error, "File: _write: {}", strerror(errno));
        _close(fd);
        return false;
    }

    stream = _fdopen(fd, mode.data());
    if (!stream)
    {
        _close(fd);
        return false;
    }
    /*File descriptors passed into _fdopen are owned by the returned FILE * stream. If _fdopen is successful,
     * do not call _close on the file descriptor.Calling fclose on the returned FILE * also closes the file
     * descriptor.*/
    rewind(stream);
#else
    stream = fmemopen(_buf_ptr, _buf_size, mode.data());
    if (stream == nullptr)
        return false;
#endif
    return this->set_stream(stream, true);
}

long File::tell()
{
    long ret = ftell(_file_ptr);
    if (ret < 0)
        SIHD_LOG(error, "File: tell: {}", strerror(errno));
    return ret;
}

bool File::seek(long offset)
{
    return this->_seek(offset, SEEK_CUR);
}

bool File::seek_begin(long offset)
{
    return this->_seek(offset, SEEK_SET);
}

bool File::seek_end(long offset)
{
    return this->_seek(offset, SEEK_END);
}

bool File::_seek(long offset, int origin)
{
    int ret = fseek(_file_ptr, offset, origin);
    if (ret < 0)
        SIHD_LOG(error, "File: seek: {}", strerror(errno));
    return ret == 0;
}

ssize_t File::read(void *buf, size_t size)
{
    if (this->eof())
        return 0;
    size_t ret = fread(buf, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return (ssize_t)ret;
}

ssize_t File::read(std::string & str, size_t size)
{
    str.resize(size);
    ssize_t ret = this->read(reinterpret_cast<char *>(str.data()), size);
    if (ret >= 0)
        str.resize((size_t)ret);
    else
        str.resize(0);
    return ret;
}

ssize_t File::read(IArray & array)
{
    ssize_t ret = this->read(reinterpret_cast<char *>(array.buf()), array.byte_capacity());
    if (ret >= 0 && array.byte_resize((size_t)ret) == false)
    {
        array.byte_resize(0);
        return -1;
    }
    return ret;
}

ssize_t File::write(const void *data, size_t size)
{
    size_t ret = fwrite(data, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return (ssize_t)ret;
}

ssize_t File::write(ArrCharView view)
{
    return this->write(view.data(), view.size());
}

bool File::write_char(int c)
{
    return fputc(c, _file_ptr) == c;
}

ssize_t File::write_unlocked(const void *data, size_t size)
{
    size_t ret = fwrite_unlocked(data, sizeof(char), size, _file_ptr);
    if (this->error())
        return -1;
    return (ssize_t)ret;
}

ssize_t File::write_unlocked(ArrCharView view)
{
    return this->write_unlocked(view.data(), view.size());
}

bool File::write_char_unlocked(int c)
{
    return fputc_unlocked(c, _file_ptr) == c;
}

ssize_t File::read_line(char **line, size_t *size)
{
#if !defined(__SIHD_WINDOWS__)
    return getline(line, size, _file_ptr);
#else
# pragma message("File::read_line is not supported on windows")
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
# pragma message("File::read_line_delim is not supported on windows")
    *line = nullptr;
    *size = 0;
    (void)delim;
    return -1;
#endif
}

} // namespace sihd::util
