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
    this->_clear_line();
    // check if linefeed in buffer
    if (this->_find_in_last_read())
        return true;
    // before getting a read - check if file is ended
    if (_file.eof())
    {
        // if there is no remaining buffer - quit
        if (_last_read.remaining == 0)
            return false;
        // get the last of the remaining buffer with no line feed
        _total_line_size = _last_read.total - _last_read.last_index;
        _last_read.line_feed_pos = _total_line_size;
        this->_build_line();
        return true;
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
    ssize_t line_feed_at = this->_search_line_feed(data + _last_read.last_index);
    if (line_feed_at < 0)
        return false;
    // line is from '_last_read.last_index' and size 'line_feed_at'
    _last_read.line_feed_pos = line_feed_at;
    _total_line_size = line_feed_at;
    this->_build_line();
    return true;
}

void    LineReader::_build_line()
{
    if (_total_line_size < 0)
        return ;
    // allocate returned line
    _line_ptr = new char[_total_line_size + 1];
    _line_ptr[0] = 0;
    _line_ptr[_total_line_size] = 0;
    this->_concat_read_queue();
    this->_process_last_read_queue();
}

void    LineReader::_concat_read_queue()
{
    char *data;
    while (_read_queue.size() > 1)
    {
        data = _read_queue.front();
        strcat(_line_ptr, data);
        delete[] data;
        _read_queue.pop();
    }
}

void    LineReader::_process_last_read_queue()
{
    // go through last read stack
    char *data = _read_queue.front();
    if (data == nullptr)
        return ;
    // concat char until line feed pos
    strncat(_line_ptr, data + _last_read.last_index, _last_read.line_feed_pos);
    // prepare next read
    _last_read.remaining -= _last_read.line_feed_pos + 1;
    if (_last_read.remaining > 0)
        _last_read.last_index += _last_read.line_feed_pos + 1;
    else
    {
        // line is all used up - free and pop last queue element
        delete[] data;
        _read_queue.pop();
        _last_read.remaining = 0;
        _last_read.last_index = 0;
    }
    _last_read.line_feed_pos = 0;
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
        _last_read.remaining = read_ret;
        _last_read.total = read_ret;
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
        // search line feed
        ssize_t line_feed_at = this->_search_line_feed(data);
        if (line_feed_at >= 0)
        {
            // if found break loop and make line
            _total_line_size += line_feed_at;
            _last_read.line_feed_pos = line_feed_at;
            break ;
        }
        else
        {
            // add size to the total
            _total_line_size += size;
            // set line feed pos to the end of string in case of no new line feed
            _last_read.line_feed_pos = size;
        }
    }
    // if read yielded a line_feed or if read got to the end with no line feed
    if (size > 0 || _total_line_size > 0)
        this->_build_line();
    return size > 0 || _total_line_size > 0;
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
    this->_clear_line();
    this->_clear_read_data();
    memset(&_last_read, 0, sizeof(LineReader::LastRead));
    _total_line_size = 0;
}

}