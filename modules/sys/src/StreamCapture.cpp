#include <stdio.h>

#include <stdexcept>

#include <sihd/sys/StreamCapture.hpp>

#if defined(__SIHD_WINDOWS__)
# include <io.h>
# define sihd_dup _dup
# define sihd_dup2 _dup2
# define sihd_close _close
# define sihd_fileno _fileno
#else
# include <unistd.h>
# define sihd_dup ::dup
# define sihd_dup2 ::dup2
# define sihd_close ::close
# define sihd_fileno ::fileno
#endif

namespace sihd::sys
{

namespace
{

std::string read_all(std::FILE *file)
{
    if (std::fseek(file, 0, SEEK_END) != 0)
        return "";
    const long size = std::ftell(file);
    if (size <= 0)
        return "";
    std::string out(size, '\0');
    std::rewind(file);
    const size_t read = std::fread(out.data(), 1, (size_t)size, file);
    out.resize(read);
    return out;
}

} // namespace

StreamCapture::StreamCapture(std::FILE *stream): _stream(stream), _stream_fd(-1), _saved_fd(-1), _tmp_file(nullptr)
{
    if (_stream == nullptr)
        throw std::invalid_argument("cannot capture null stream");
    // flush pending writes so the capture starts empty
    std::fflush(_stream);

    _stream_fd = sihd_fileno(_stream);
    if (_stream_fd == -1)
        throw std::runtime_error("stream has no file descriptor");

    _tmp_file = std::tmpfile();
    if (_tmp_file == nullptr)
        throw std::runtime_error("cannot create temporary file");

    _saved_fd = sihd_dup(_stream_fd);
    if (_saved_fd == -1)
    {
        std::fclose(_tmp_file);
        _tmp_file = nullptr;
        throw std::runtime_error("cannot duplicate stream file descriptor");
    }

    if (sihd_dup2(sihd_fileno(_tmp_file), _stream_fd) == -1)
    {
        sihd_close(_saved_fd);
        _saved_fd = -1;
        std::fclose(_tmp_file);
        _tmp_file = nullptr;
        throw std::runtime_error("cannot redirect stream file descriptor");
    }
}

StreamCapture::~StreamCapture()
{
    if (_tmp_file == nullptr)
        return;

    std::fflush(_stream);
    sihd_dup2(_saved_fd, _stream_fd);
    sihd_close(_saved_fd);
    std::fclose(_tmp_file);
}

std::string StreamCapture::str()
{
    std::fflush(_stream);
    return read_all(_tmp_file);
}

} // namespace sihd::sys

#undef sihd_dup
#undef sihd_dup2
#undef sihd_close
#undef sihd_fileno
