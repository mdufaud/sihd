#include <string.h>

#include <sihd/sys/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;
namespace sihd::sys
{

SIHD_LOGGER;

LineReader::LineReader(const LineReaderOptions & options):
    _read_buff_size(options.read_buffsize),
    _read_ptr(nullptr),
    _line_buff_size(options.line_buffsize),
    _line_size(0),
    _line_ptr(nullptr),
    _last_read_index(0),
    _read_size(0),
    _error(false),
    _put_delimiter_in_line(options.delimiter_in_line),
    _delimiter(options.delimiter)
{
}

LineReader::LineReader(std::string_view path, const LineReaderOptions & options): LineReader(options)
{
    this->open(path);
}

LineReader::LineReader(int fd, const LineReaderOptions & options): LineReader(options)
{
    this->open_fd(fd);
}

LineReader::LineReader(FILE *stream, bool ownership, const LineReaderOptions & options): LineReader(options)
{
    this->set_stream(stream, ownership);
}

LineReader::~LineReader()
{
    this->_delete_buffers();
}

bool LineReader::_init()
{
    bool ret = _line_ptr != nullptr || this->_allocate_line();
    ret = ret && (_read_ptr != nullptr || this->_allocate_read_buffer());
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
    return this->_allocate_read_buffer();
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
    return this->_allocate_line();
}

bool LineReader::open(std::string_view path)
{
    return this->_init() && _file.open(path, "r");
}

bool LineReader::open_fd(int fd)
{
    return this->_init() && _file.open_fd(fd, "r");
}

bool LineReader::set_stream(FILE *stream, bool ownership)
{
    return this->_init() && _file.set_stream(stream, ownership);
}

bool LineReader::is_open() const
{
    return _file.is_open();
}

bool LineReader::close()
{
    this->_reset();
    return _file.close();
}

bool LineReader::read_next()
{
    _line_ptr[0] = 0;
    size_t fill_idx = 0;
    while (1)
    {
        if ((ssize_t)_last_read_index < _read_size)
        {
            // look for delimiter
            size_t copy_len = 0;
            const char *match = (const char *)memchr(_read_ptr + _last_read_index,
                                                     _delimiter,
                                                     _read_size - _last_read_index);
            if (match != nullptr)
                copy_len = (match - (_read_ptr + _last_read_index)) + (int)(_put_delimiter_in_line);
            else
                copy_len = _read_size - _last_read_index;
            // if length to copy is too much for line buffer - reallocate
            if ((fill_idx + copy_len) >= _line_buff_size)
            {
                if (this->_reallocate_line(fill_idx + copy_len) == false)
                {
                    _error = true;
                    return false;
                }
            }
            // copy from into line either full read buffer or just matching part
            memcpy(_line_ptr + fill_idx, _read_ptr + _last_read_index, copy_len);
            // prepare next loop/call
            _last_read_index += copy_len + (int)(!_put_delimiter_in_line);
            fill_idx += copy_len;
            // return if delimiter found
            if (match != nullptr)
            {
                _line_size = fill_idx;
                _line_ptr[fill_idx] = 0;
                return true;
            }
        }
        // new read buffer
        _read_size = _file.read(_read_ptr, _read_buff_size);
        _last_read_index = 0;
        // exit if read error
        if (_read_size == -1)
        {
            _error = true;
            return false;
        }
        _read_ptr[_read_size] = 0;
        // end
        if (_read_size == 0)
        {
            _line_size = fill_idx;
            // prepare next call
            // fill_idx == 0 means no more to read
            _line_ptr[fill_idx] = 0;
            _last_read_index = fill_idx;
            return fill_idx > 0;
        }
    }
    return false;
}

bool LineReader::get_read_data(ArrCharView & view) const
{
    view = ArrCharView {_line_ptr, _line_size};
    return _line_ptr != nullptr;
}

void LineReader::_reset()
{
    _error = false;
    _last_read_index = 0;
    _read_size = 0;
    _line_size = 0;
    if (_read_ptr != nullptr)
        _read_ptr[0] = 0;
    if (_line_ptr != nullptr)
        _line_ptr[0] = 0;
}

bool LineReader::_reallocate_line(size_t needed)
{
    _line_buff_size = needed;
    return this->_allocate_line();
}

bool LineReader::_allocate_line()
{
    _line_ptr = (char *)realloc(_line_ptr, _line_buff_size + 1);
    if (_line_ptr == nullptr)
        SIHD_LOG(error, "LineReader: error allocating line");
    return _line_ptr != nullptr;
}

bool LineReader::_allocate_read_buffer()
{
    _read_ptr = (char *)realloc(_read_ptr, _read_buff_size + 1);
    if (_read_ptr == nullptr)
        SIHD_LOG(error, "LineReader: error allocating read buffer");
    return _read_ptr != nullptr;
}

void LineReader::_delete_buffers()
{
    if (_line_ptr != nullptr)
        free(_line_ptr);
    if (_read_ptr != nullptr)
        free(_read_ptr);
    _line_ptr = nullptr;
    _read_ptr = nullptr;
}

bool LineReader::fast_read_line(std::string & line, FILE *stream, const LineReaderOptions & options)
{
    ArrCharView view;
    LineReader reader(options);

    if (reader.set_stream(stream, false) && reader.read_next() && reader.get_read_data(view))
    {
        line.assign(view.data(), view.size());
        return true;
    }
    return false;
}

bool LineReader::fast_read_stdin(std::string & line, LineReaderOptions options)
{
    ArrCharView view;
    options.read_buffsize = 1;
    LineReader reader(options);

    if (reader.set_stream(stdin, false) && reader.read_next() && reader.get_read_data(view))
    {
        line.assign(view.data(), view.size());
        return true;
    }
    return false;
}

} // namespace sihd::sys
