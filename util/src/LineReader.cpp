#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Str.hpp>
#include <string.h>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(LineReader)

LOGGER;

LineReader::LineReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
    _read_buff_size = 4096;
    _line_ptr = nullptr;
    _line_buff_step = 512;
    _line_buff_size = 0;
    this->_reallocate_line();
    this->_reset();
}

LineReader::~LineReader()
{
    this->_clear_read_data();
    this->_clear_line();
}

bool    LineReader::set_read_buffsize(size_t buff)
{
    if (this->is_open())
    {
        LOG(error, "LineReader: cannot set buffer size on opened file");
        return false;
    }
    if (buff == 0)
    {
        LOG(error, "LineReader: cannot set buffer size to 0");
        return false;
    }
    _read_buff_size = buff;
    return true;
}

bool    LineReader::open(const std::string & path)
{
    return _file.open(path, "r");
}

bool    LineReader::is_open() const
{
    return _file.is_open();
}

bool    LineReader::close()
{
    this->_reset();
    return _file.close();
}

bool    LineReader::read_next()
{
    //this->_clear_line();
    // check if linefeed in buffer
    if (this->_find_in_last_read())
        return true;
    // before getting a read - check if file is ended
    if (_file.eof())
    {
        // if there is no remaining buffer - quit
        if (_back_read.remaining == 0)
            return false;
        // get the last of the remaining buffer with no line feed

        //_total_line_size = _last_read.total - _last_read.last_index;

        _back_read.line_feed_pos = _back_read.total - _back_read.last_index;
        // TRACE("IN END");
        return this->_build_line();
    }
    // search in read
    return this->_find_in_read();
}

bool    LineReader::get_read_data(char **data, size_t *size) const
{
    *data = _line_ptr;
    *size = _total_line_size;
    return _line_ptr != nullptr;
}

bool    LineReader::_find_in_last_read()
{
    if (_read_queue.empty())
        return false;
    // get only element in queue (should be)
    char *data = _read_queue.front();
    ssize_t line_feed_at = this->_search_line_feed(data + _back_read.last_index);
    if (line_feed_at < 0)
        return false;
    // line is from '_last_read.last_index' and size 'line_feed_at'
    _back_read.line_feed_pos = line_feed_at;
    // TRACE("IN LAST READ");
    return this->_build_line();
}

bool    LineReader::_build_line()
{
    _total_line_size = _total_read_size - _back_read.total + _back_read.line_feed_pos
                        - (_front_read.last_index);
    if ((ssize_t)_line_buff_size <= _total_line_size)
    {
        if (this->_reallocate_line() == false)
            return false;
    }
    _line_ptr[0] = 0;
    _line_ptr[_total_line_size] = 0;
    this->_concat_read_queue();
    this->_process_last_read_queue();
    return true;
}

void    LineReader::_concat_read_queue()
{
    char *data;
    if (_front_read.last_index > 0)
    {
        data = _read_queue.front();
        strcat(_line_ptr, data + _front_read.last_index);
        delete[] data;
        _read_queue.pop();
        _total_read_size -= _front_read.total;
        memset(&_front_read, 0, sizeof(LineReader::ReadSave));
    }
    // size_t tot = 0;
    while (_read_queue.size() > 1)
    {
        data = _read_queue.front();
        strcat(_line_ptr, data);
        delete[] data;
        _read_queue.pop();
        _total_read_size -= _read_buff_size;
    }
}

void    LineReader::_process_last_read_queue()
{
    // go through last read stack
    char *data = _read_queue.front();
    if (data == nullptr)
        return ;
    // concat char until line feed pos
    strncat(_line_ptr, data + _back_read.last_index, _back_read.line_feed_pos);
    // prepare next read
    _back_read.remaining -= _back_read.line_feed_pos + 1;
    if (_back_read.remaining > 0)
        _back_read.last_index += _back_read.line_feed_pos + 1;
    else
    {
        // line is all used up - free and pop last queue element
        delete[] data;
        _read_queue.pop();
        _back_read.remaining = 0;
        _back_read.last_index = 0;
        _total_read_size = 0;
    }
    _back_read.line_feed_pos = 0;
}

ssize_t LineReader::_add_new_read()
{
    // allocate new read
    char *data = new char[_read_buff_size + 1];
    if (data == nullptr)
    {
        LOG(error, "LineReader: allocation error");
        return -1;
    }
    data[0] = 0;
    // fill a stack with a read
    ssize_t read_ret = _file.read(data, _read_buff_size);
    if (read_ret == -1)
    {
        LOG(error, "LineReader: error reading file");
        return -1;
    }
    else if (read_ret == 0)
        delete[] data;
    else
    {
        data[read_ret] = 0;
        if (_back_read.remaining > 0)
        {
            _front_read = _back_read;
        }
        _back_read.remaining = read_ret;
        _back_read.total = read_ret;
        _back_read.last_index = 0;
        _read_queue.push(data);

    }
    return read_ret;
}

bool    LineReader::_find_in_read()
{
    char *data;
    ssize_t size;
    // query new read buffer
    while ((size = this->_add_new_read()) > 0)
    {
        // analyse last read
        data = _read_queue.back();
        _total_read_size += size;
        // search line feed
        ssize_t line_feed_at = this->_search_line_feed(data);
        if (line_feed_at >= 0)
        {
            // if found break loop and make line
            _back_read.line_feed_pos = line_feed_at;
            break ;
        }
        else
        {
            // set line feed pos to the end of string in case of no new line feed
            _back_read.line_feed_pos = size;
        }
    }
    // if read yielded a line_feed or if read got to the end with no line feed
    if (size > 0 || _total_read_size > 0)
        return this->_build_line();
    return false;
}

ssize_t LineReader::_search_line_feed(const char *data)
{
    const char *pos = strchr(data, '\n');
    if (pos == nullptr)
        return -1;
    return pos - data;
}

void    LineReader::_clear_line()
{
    _total_line_size = 0;
    if (_line_ptr)
    {
        delete[] _line_ptr;
        _line_ptr = nullptr;
    }
}

void    LineReader::_clear_read_data()
{
    while (!_read_queue.empty())
    {
        char *data = _read_queue.front();
        delete[] data;
        _read_queue.pop();
    }
}

void    LineReader::_reset()
{
    // this->_clear_line();
    this->_clear_read_data();
    memset(&_back_read, 0, sizeof(LineReader::ReadSave));
    memset(&_front_read, 0, sizeof(LineReader::ReadSave));
    _total_line_size = 0;
    _total_read_size = 0;
}

bool    LineReader::_reallocate_line()
{
    _line_buff_size += _line_buff_step;
    if (_line_ptr == nullptr)
        _line_ptr = (char *)malloc(_line_buff_size);
    else
        _line_ptr = (char *)realloc(_line_ptr, _line_buff_size);
    if (_line_ptr == nullptr)
        LOG(error, "LineReader: error allocating line");
    return _line_ptr != nullptr;
}

}