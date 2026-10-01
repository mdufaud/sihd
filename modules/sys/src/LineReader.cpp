#include <cstring>

#include <sihd/sys/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;
namespace sihd::sys
{

SIHD_LOGGER;

LineReader::LineReader(const LineReaderOptions & options):
    _read_buff_size(options.read_buffsize),
    _line_buff_size(options.line_buffsize),
    _line_size(0),
    _last_read_index(0),
    _read_size(0),
    _put_delimiter_in_line(options.delimiter_in_line),
    _delimiter(options.delimiter)
{
}

LineReader::LineReader(std::string_view path, const LineReaderOptions & options): LineReader(options)
{
    SIHD_UNEXPECTED_LOG(this->open(path));
}

LineReader::LineReader(int fd, const LineReaderOptions & options): LineReader(options)
{
    SIHD_UNEXPECTED_LOG(this->open_fd(fd));
}

LineReader::LineReader(FILE *stream, bool ownership, const LineReaderOptions & options): LineReader(options)
{
    SIHD_UNEXPECTED_LOG(this->set_stream(stream, ownership));
}

bool LineReader::_init()
{
    bool ret = _line_buff.data() != nullptr || this->_allocate_line();
    ret = ret && (_read_buff.data() != nullptr || this->_allocate_read_buffer());
    if (ret)
        this->_reset();
    return ret;
}

bool LineReader::set_delimiter_in_line(bool active)
{
    _put_delimiter_in_line = active;
    return true;
}

bool LineReader::set_delimiter(int c)
{
    _delimiter = c;
    return true;
}

bool LineReader::set_read_buffsize(size_t buff)
{
    if (_read_buff_size == buff)
        return true;
    if (buff == 0)
    {
        SIHD_LOG(error, "LineReader: cannot set read buffer size to 0");
        return false;
    }
    _read_buff_size = buff;
    if (this->_allocate_read_buffer() == false)
        return false;
    this->_reset();
    return true;
}

bool LineReader::set_line_buffsize(size_t buff)
{
    if (_line_buff_size == buff)
        return true;
    if (buff == 0)
    {
        SIHD_LOG(error, "LineReader: cannot set line buffer size to 0");
        return false;
    }
    _line_buff_size = buff;
    if (this->_allocate_line() == false)
        return false;
    this->_reset();
    return true;
}

std::expected<void, Error> LineReader::open(std::string_view path)
{
    if (this->_init() == false)
        return std::unexpected(Error(out_of_memory, "LineReader: could not allocate buffers"));
    return _file.open(path, "r");
}

std::expected<void, Error> LineReader::open_fd(int fd)
{
    if (this->_init() == false)
        return std::unexpected(Error(out_of_memory, "LineReader: could not allocate buffers"));
    return _file.open_fd(fd, "r");
}

std::expected<void, Error> LineReader::set_stream(FILE *stream, bool ownership)
{
    if (this->_init() == false)
        return std::unexpected(Error(out_of_memory, "LineReader: could not allocate buffers"));
    return _file.set_stream(stream, ownership);
}

bool LineReader::is_open() const
{
    return _file.is_open();
}

bool LineReader::close()
{
    this->_reset();
    return !SIHD_UNEXPECTED_LOG(_file.close());
}

std::expected<bool, Error> LineReader::read_next()
{
    if (_line_buff.data() == nullptr || _read_buff.data() == nullptr)
        return std::unexpected(Error(not_initialized, "LineReader: buffers not allocated"));
    _line_buff.data()[0] = 0;
    size_t fill_idx = 0;
    while (1)
    {
        if (static_cast<ssize_t>(_last_read_index) < _read_size)
        {
            // look for delimiter
            size_t copy_len = 0;
            const char *read_at = _read_buff.data() + _last_read_index;
            const char *match = static_cast<const char *>(memchr(read_at, _delimiter, _read_size - _last_read_index));
            if (match != nullptr)
                copy_len = (match - read_at) + static_cast<size_t>(_put_delimiter_in_line);
            else
                copy_len = _read_size - _last_read_index;
            // if length to copy is too much for line buffer - reallocate
            if ((fill_idx + copy_len) >= _line_buff_size)
            {
                if (this->_reallocate_line(fill_idx + copy_len) == false)
                    return std::unexpected(Error(out_of_memory, "LineReader: could not reallocate line buffer"));
            }
            // copy from into line either full read buffer or just matching part
            memcpy(_line_buff.data() + fill_idx, _read_buff.data() + _last_read_index, copy_len);
            // prepare next loop/call
            _last_read_index += copy_len + static_cast<size_t>(!_put_delimiter_in_line);
            fill_idx += copy_len;
            // return if delimiter found
            if (match != nullptr)
            {
                _line_size = fill_idx;
                _line_buff.data()[fill_idx] = 0;
                return true;
            }
        }
        // new read buffer
        auto read = _file.read(_read_buff.data(), _read_buff_size);
        SIHD_UNEXPECTED_RETURN(read);
        _read_size = (ssize_t)*read;
        _last_read_index = 0;
        _read_buff.data()[_read_size] = 0;
        // end
        if (_read_size == 0)
        {
            _line_size = fill_idx;
            // prepare next call
            // fill_idx == 0 means no more to read
            _line_buff.data()[fill_idx] = 0;
            _last_read_index = fill_idx;
            return fill_idx > 0;
        }
    }
}

bool LineReader::get_read_data(ArrCharView & view) const
{
    view = ArrCharView {_line_buff.data(), _line_size};
    return _line_buff.data() != nullptr;
}

void LineReader::_reset()
{
    _last_read_index = 0;
    _read_size = 0;
    _line_size = 0;
    if (_read_buff.data() != nullptr)
        _read_buff.data()[0] = 0;
    if (_line_buff.data() != nullptr)
        _line_buff.data()[0] = 0;
}

bool LineReader::_reallocate_line(size_t needed)
{
    _line_buff_size = needed;
    return this->_allocate_line();
}

bool LineReader::_allocate_line()
{
    return _line_buff.reserve(_line_buff_size + 1);
}

bool LineReader::_allocate_read_buffer()
{
    return _read_buff.reserve(_read_buff_size + 1);
}

std::expected<std::string, Error> LineReader::fast_read_line(FILE *stream, const LineReaderOptions & options)
{
    LineReader reader(options);

    auto opened = reader.set_stream(stream, false);
    SIHD_UNEXPECTED_RETURN(opened);
    auto line = reader.read_next();
    SIHD_UNEXPECTED_RETURN(line);
    if (!*line)
        return std::unexpected(Error(not_found, "no line to read"));
    ArrCharView view;
    reader.get_read_data(view);
    return std::string(view.data(), view.size());
}

std::expected<std::string, Error> LineReader::fast_read_stdin(LineReaderOptions options)
{
    options.read_buffsize = 1;
    return LineReader::fast_read_line(stdin, options);
}

} // namespace sihd::sys
