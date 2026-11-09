#include <fcntl.h>
#include <sys/stat.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <new>
#include <random>
#include <string>

#include <sihd/sys/File.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

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
# define file_seek _fseeki64
# define file_tell _ftelli64
#else
# include <unistd.h>
# define file_seek fseeko
# define file_tell ftello
static_assert(sizeof(off_t) == sizeof(int64_t));
#endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_NEW_LOGGER("sihd::sys");

#if defined(__SIHD_WINDOWS__)

namespace
{

// mingw-w64 does not ship getline/getdelim, so provide a portable equivalent
ssize_t win_getdelim(char **line, size_t *size, int delim, FILE *stream)
{
    if (line == nullptr || size == nullptr || stream == nullptr)
    {
        errno = EINVAL;
        return -1;
    }
    if (*line == nullptr || *size == 0)
    {
        char *allocated = (char *)realloc(*line, 128);

        if (allocated == nullptr)
            return -1;
        *line = allocated;
        *size = 128;
    }
    size_t pos = 0;
    int chr;

    while ((chr = fgetc(stream)) != EOF)
    {
        if (pos + 1 >= *size)
        {
            const size_t new_size = *size * 2;
            char *new_line = (char *)realloc(*line, new_size);
            if (new_line == nullptr)
                return -1;
            *line = new_line;
            *size = new_size;
        }
        (*line)[pos++] = (char)chr;
        if (chr == delim)
            break;
    }
    if (pos == 0 && chr == EOF)
        return -1;
    (*line)[pos] = '\0';
    return (ssize_t)pos;
}

} // namespace

#endif

namespace
{

std::unexpected<Error> no_file_open()
{
    return std::unexpected(Error(not_initialized, "no file open"));
}

} // namespace

File::File()
{
    _file_ptr = nullptr;
    _buf_ptr = nullptr;
    _buf_size = 0;
    _buf_mode = _IOLBF;
    _stream_ownership = true;
}

File::File(int fd, const std::string & mode): File()
{
    SIHD_UNEXPECTED_LOG(this->open_fd(fd, mode));
}

File::File(FILE *stream, bool ownership): File()
{
    SIHD_UNEXPECTED_LOG(this->set_stream(stream, ownership));
}

File::File(const std::string & path, const std::string & mode): File()
{
    SIHD_UNEXPECTED_LOG(this->open(path, mode));
}

File::File(File && other): File()
{
    *this = std::move(other);
}

File::~File()
{
    (void)this->close();
    this->_delete_buffer();
}

File & File::operator=(File && other)
{
    if (this == &other)
        return *this;
    (void)this->close();
    this->_delete_buffer();
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

Result<void> File::set_buffer_size(size_t size)
{
    if (_buf_size == size)
        return {};
    if (size == 0)
        return std::unexpected(Error(invalid_argument, "cannot set buffer size to 0"));
    // an open stream may still write into the current buffer
    if (_file_ptr != nullptr)
        return std::unexpected(Error(invalid_argument, "cannot resize the buffer of an open file"));
    this->_delete_buffer();
    _buf_size = size;
    if (this->_allocate_buffer_if_not_exists() == false)
        return std::unexpected(Error(out_of_memory, "could not allocate buffer of {} bytes", _buf_size));
    return {};
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
        _buf_ptr = new (std::nothrow) char[_buf_size];
        if (_buf_ptr != nullptr)
        {
            _buf_ptr[0] = 0;
            _buf_ptr[_buf_size - 1] = 0;
        }
    }
    return _buf_size == 0 || _buf_ptr != nullptr;
}

Result<void> File::buff_stream()
{
    if (!this->_allocate_buffer_if_not_exists())
        return std::unexpected(Error(out_of_memory, "could not allocate buffer of {} bytes", _buf_size));
    if (_file_ptr != nullptr && _buf_ptr != nullptr)
    {
        int ret = setvbuf(_file_ptr, _buf_ptr, _buf_mode, _buf_size);
        if (ret < 0)
            return std::unexpected(Error::from_errno("could not set stream buffer"));
    }
    return {};
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

Result<void> File::open_fd(int fd, const std::string & mode)
{
    FILE *opened = fdopen(fd, mode.c_str());
    if (opened == nullptr)
        return std::unexpected(Error::from_errno("could not open file descriptor {}", fd));
    return this->_replace_stream(opened, "");
}

Result<void> File::set_stream(FILE *stream, bool ownership)
{
    auto closed = this->close();
    if (!closed)
        return closed;
    _file_ptr = stream;
    _stream_ownership = ownership;
    return {};
}

Result<void> File::open_tmpfile()
{
    FILE *opened = tmpfile();
    if (opened == nullptr)
        return std::unexpected(Error::from_errno("could not open temporary file"));
    return this->_replace_stream(opened, "");
}

Result<void> File::open_tmp(const std::string & prefix, bool write_binary, const std::string & suffix)
{
    const size_t path_size = prefix.size() + 6 + suffix.size();
    if (path_size >= PATH_MAX)
        return std::unexpected(Error(invalid_argument, "path too long: {}", path_size));
    char path[PATH_MAX];
    strcpy(path, prefix.c_str());
    strcpy(path + prefix.size(), "XXXXXX");
    strcpy(path + prefix.size() + 6, suffix.c_str());
#if !defined(__SIHD_WINDOWS__)
    int fd = mkstemps(path, (int)suffix.size());
    if (fd < 0)
        return std::unexpected(Error::from_errno("could not open temporary file"));
    FILE *opened = fdopen(fd, write_binary ? "wb" : "w");
    if (opened == nullptr)
    {
        auto error = Error::from_errno("could not open temporary file");
        ::close(fd);
        ::unlink(path);
        return std::unexpected(std::move(error));
    }
    auto replaced = this->_replace_stream(opened, path);
    if (!replaced)
        ::unlink(path);
    return replaced;
#else
    // _mktemp_s derives 5 digits from the PID plus one letter (26 names max) and is broken under wine
    // (EEXIST on first call): generate the XXXXXX part ourselves, create with "x", retry on collision
    static constexpr std::string_view charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    thread_local std::mt19937 gen(std::random_device {}());
    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        for (size_t i = 0; i < 6; ++i)
            path[prefix.size() + i] = charset[dist(gen)];
        FILE *opened = fopen(path, write_binary ? "wbx" : "wx");
        if (opened != nullptr)
        {
            auto replaced = this->_replace_stream(opened, path);
            if (!replaced)
                std::remove(path);
            return replaced;
        }
        if (errno != EEXIST)
            break;
    }
    return std::unexpected(Error::from_errno("could not open temporary file"));
#endif
}

Result<void> File::open(const std::string & path, const std::string & mode)
{
    // a refused path keeps the open file; flushed first so its close cannot write into a file the new open truncated
    if (_file_ptr != nullptr && fflush(_file_ptr) != 0)
        return std::unexpected(Error::from_errno("could not flush before opening '{}'", path));
    FILE *opened = fopen(path.c_str(), mode.c_str());
    if (opened == nullptr)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    return this->_replace_stream(opened, path);
}

Result<void> File::open(const std::string & path, const OpenOptions & options)
{
    if (!options.read && !options.write)
        return std::unexpected(Error(invalid_argument, "a file opens to read, to write or both"));
    if (!options.write && (options.truncate || options.append))
        return std::unexpected(Error(invalid_argument, "truncate and append need write"));
    if (options.exclusive && !options.create)
        return std::unexpected(Error(invalid_argument, "exclusive needs create"));

    int flags = O_RDONLY;
    if (options.write)
        flags = options.read ? O_RDWR : O_WRONLY;
    if (options.create)
        flags |= O_CREAT;
    if (options.truncate)
        flags |= O_TRUNC;
    if (options.exclusive)
        flags |= O_EXCL;
    if (options.append)
        flags |= O_APPEND;
#if defined(O_BINARY)
    flags |= O_BINARY;
#endif
    // fdopen never creates nor truncates: its mode only matches the descriptor access
    std::string mode = "rb";
    if (options.write && options.append)
        mode = options.read ? "a+b" : "ab";
    else if (options.write)
        mode = options.read ? "r+b" : "wb";

    if (_file_ptr != nullptr && fflush(_file_ptr) != 0)
        return std::unexpected(Error::from_errno("could not flush before opening '{}'", path));
    const int fd = ::open(path.c_str(), flags, options.permissions);
    if (fd < 0)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    FILE *opened = fdopen(fd, mode.c_str());
    if (opened == nullptr)
    {
        auto error = Error::from_errno("could not open '{}'", path);
        ::close(fd);
        return std::unexpected(std::move(error));
    }
    return this->_replace_stream(opened, path);
}

Result<void> File::_replace_stream(FILE *opened, const std::string & path)
{
    auto closed = this->close();
    if (!closed)
    {
        fclose(opened);
        return closed;
    }
    _file_ptr = opened;
    _path = path;
    _stream_ownership = true;
    return {};
}

bool File::is_open() const
{
    return _file_ptr != nullptr;
}

bool File::eof() const
{
    return _file_ptr == nullptr || feof(_file_ptr) != 0;
}

bool File::eof_unlocked() const
{
    return _file_ptr == nullptr || feof_unlocked(_file_ptr) != 0;
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
    return _file_ptr == nullptr ? 0 : ferror(_file_ptr);
}

int File::error_unlocked() const
{
    return _file_ptr == nullptr ? 0 : ferror_unlocked(_file_ptr);
}

void File::clear_errors()
{
    if (_file_ptr != nullptr)
        clearerr(_file_ptr);
}

Result<void> File::flush()
{
    if (_file_ptr == nullptr)
        return no_file_open();
    if (fflush(_file_ptr) != 0)
        return std::unexpected(Error::from_errno("could not flush file"));
    return {};
}

Result<void> File::flush_unlocked()
{
    if (_file_ptr == nullptr)
        return no_file_open();
    if (fflush_unlocked(_file_ptr) != 0)
        return std::unexpected(Error::from_errno("could not flush file"));
    return {};
}

Result<void> File::close()
{
    if (_file_ptr != nullptr)
    {
        if (_stream_ownership && fclose(_file_ptr) != 0)
        {
            _file_ptr = nullptr;
            _path.clear();
            return std::unexpected(Error::from_errno("could not close file"));
        }
        _file_ptr = nullptr;
        _path.clear();
    }
    return {};
}

Result<int64_t> File::file_size()
{
    auto current_offset = this->tell();
    SIHD_UNEXPECTED_RETURN(current_offset);
    auto seek_res = this->seek_end(0);
    SIHD_UNEXPECTED_RETURN(seek_res);
    auto size = this->tell();
    SIHD_UNEXPECTED_LOG(this->seek_begin(*current_offset));
    SIHD_UNEXPECTED_RETURN(size);
    return *size;
}

void File::lock()
{
    if (_file_ptr == nullptr)
        return;
#if !defined(__SIHD_WINDOWS__)
    flockfile(_file_ptr);
#else
    _lock_file(_file_ptr);
#endif
}

bool File::trylock()
{
    if (_file_ptr == nullptr)
        return false;
#if !defined(__SIHD_WINDOWS__)
    return ftrylockfile(_file_ptr) == 0;
#else
    _lock_file(_file_ptr);
    return false;
#endif
}

void File::unlock()
{
    if (_file_ptr == nullptr)
        return;
#if !defined(__SIHD_WINDOWS__)
    funlockfile(_file_ptr);
#else
    _unlock_file(_file_ptr);
#endif
}

Result<void> File::open_mem(const std::string & mode, std::string_view put_in_buffer)
{
    if (_buf_size == 0)
        return std::unexpected(Error(not_initialized, "set_buffer_size before open_mem"));
    if (this->_allocate_buffer_if_not_exists() == false)
        return std::unexpected(Error(out_of_memory, "could not allocate buffer of {} bytes", _buf_size));

    if (put_in_buffer.empty() == false)
    {
        const size_t max_len = std::min(_buf_size, put_in_buffer.size());

        memcpy(_buf_ptr, put_in_buffer.data(), max_len);

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
        return std::unexpected(Error(io_error, "could not get temporary path: {}", os::last_error_str()));
    if (!GetTempFileNameA(tp, tfname, 0, fn))
        return std::unexpected(Error(io_error, "could not get temporary file name: {}", os::last_error_str()));
    retner = _sopen_s(pfd,
                      fn,
                      _O_CREAT | _O_SHORT_LIVED | _O_TEMPORARY | _O_RDWR | _O_BINARY | _O_NOINHERIT,
                      _SH_DENYRW,
                      _S_IREAD | _S_IWRITE);
    if (retner != 0)
        return std::unexpected(Error::from_errno("could not open temporary file"));
    if (fd == -1)
        return std::unexpected(Error(io_error, "could not open temporary file"));

    if (_write(fd, _buf_ptr, _buf_size) != (ssize_t)_buf_size)
    {
        auto error = Error::from_errno("could not write into temporary file");
        _close(fd);
        return std::unexpected(std::move(error));
    }

    stream = _fdopen(fd, mode.c_str());
    if (!stream)
    {
        auto error = Error::from_errno("could not open temporary file");
        _close(fd);
        return std::unexpected(std::move(error));
    }
    /*File descriptors passed into _fdopen are owned by the returned FILE * stream. If _fdopen is successful,
     * do not call _close on the file descriptor.Calling fclose on the returned FILE * also closes the file
     * descriptor.*/
    rewind(stream);
#else
    stream = fmemopen(_buf_ptr, _buf_size, mode.c_str());
    if (stream == nullptr)
        return std::unexpected(Error::from_errno("could not open memory stream"));
#endif
    return this->set_stream(stream, true);
}

Result<int64_t> File::tell()
{
    if (_file_ptr == nullptr)
        return no_file_open();
    const int64_t ret = file_tell(_file_ptr);
    if (ret < 0)
        return std::unexpected(Error::from_errno("could not tell position of '{}'", _path));
    return ret;
}

Result<void> File::seek(int64_t offset)
{
    return this->_seek(offset, SEEK_CUR);
}

Result<void> File::seek_begin(int64_t offset)
{
    return this->_seek(offset, SEEK_SET);
}

Result<void> File::seek_end(int64_t offset)
{
    return this->_seek(offset, SEEK_END);
}

Result<void> File::_seek(int64_t offset, int origin)
{
    if (_file_ptr == nullptr)
        return no_file_open();
    int ret = file_seek(_file_ptr, offset, origin);
    if (ret < 0)
        return std::unexpected(Error::from_errno("could not seek in '{}'", _path));
    return {};
}

Result<size_t> File::read(void *buf, size_t size)
{
    if (_file_ptr == nullptr)
        return no_file_open();
    if (this->eof())
        return 0;
    size_t ret = fread(buf, sizeof(char), size, _file_ptr);
    if (this->error())
        return std::unexpected(Error(io_error, "could not read '{}'", _path));
    return ret;
}

Result<size_t> File::read(std::string & str, size_t size)
{
    str.resize(size);
    auto read = this->read(reinterpret_cast<char *>(str.data()), size);
    if (!read)
    {
        str.resize(0);
        return read;
    }
    str.resize(*read);
    return read;
}

Result<size_t> File::read(IArray & array)
{
    auto read = this->read(reinterpret_cast<char *>(array.buf()), array.byte_capacity());
    if (!read)
        return read;
    if (array.byte_resize(*read) == false)
    {
        array.byte_resize(0);
        return std::unexpected(Error(out_of_memory, "could not resize array to {} bytes", *read));
    }
    return read;
}

Result<size_t> File::write(const void *data, size_t size)
{
    if (_file_ptr == nullptr)
        return no_file_open();
    size_t ret = fwrite(data, sizeof(char), size, _file_ptr);
    if (this->error())
        return std::unexpected(Error(io_error, "could not write into '{}'", _path));
    return ret;
}

Result<size_t> File::write(ArrCharView view)
{
    return this->write(view.data(), view.size());
}

Result<void> File::write_char(int c)
{
    if (_file_ptr == nullptr)
        return no_file_open();
    if (fputc(c, _file_ptr) != c)
        return std::unexpected(Error::from_errno("could not write char into '{}'", _path));
    return {};
}

Result<size_t> File::write_unlocked(const void *data, size_t size)
{
    if (_file_ptr == nullptr)
        return no_file_open();
    size_t ret = fwrite_unlocked(data, sizeof(char), size, _file_ptr);
    if (this->error())
        return std::unexpected(Error(io_error, "could not write into '{}'", _path));
    return ret;
}

Result<size_t> File::write_unlocked(ArrCharView view)
{
    return this->write_unlocked(view.data(), view.size());
}

Result<void> File::write_char_unlocked(int c)
{
    if (_file_ptr == nullptr)
        return no_file_open();
    if (fputc_unlocked(c, _file_ptr) != c)
        return std::unexpected(Error::from_errno("could not write char into '{}'", _path));
    return {};
}

Result<size_t> File::read_line(char **line, size_t *size)
{
    return this->read_line_delim(line, size, '\n');
}

Result<size_t> File::read_line_delim(char **line, size_t *size, int delim)
{
    if (_file_ptr == nullptr)
        return no_file_open();
#if defined(__SIHD_WINDOWS__)
    ssize_t ret = win_getdelim(line, size, delim, _file_ptr);
#else
    ssize_t ret = getdelim(line, size, delim, _file_ptr);
#endif
    if (ret < 0)
    {
        // getdelim conflates end of file and error
        if (this->eof())
            return 0;
        return std::unexpected(Error::from_errno("could not read line from '{}'", _path));
    }
    return (size_t)ret;
}

} // namespace sihd::sys
