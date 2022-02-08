#include <sihd/csv/CsvReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Str.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvReader)

SIHD_NEW_LOGGER("sihd::csv");

CsvReader::CsvReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
    _line_size = 0;
    _line_ptr = nullptr;
    _comment = '#';
    _splitter.set_delimiter(",");
    _splitter.set_empty_delimitations(true);
    _file.set_no_buffering();
    this->add_conf("quote", &CsvReader::set_quote_value);
    this->add_conf("delimiter", &CsvReader::set_delimiter);
    this->add_conf("comment", &CsvReader::set_commentary);
}

CsvReader::~CsvReader()
{
    this->_free_line();
}

bool    CsvReader::set_quote_value(int c)
{
    if (sihd::util::Str::closing_escape_of(c) < 0)
    {
        SIHD_LOG(error, "CsvReader: quote character '" << c << "' is not supported");
        return false;
    }
    _quote = c;
    _splitter.set_escape_sequences((char *)&c);
    return true;
}

bool    CsvReader::set_delimiter(int c)
{
    if (std::isprint(c) == 0)
    {
        SIHD_LOG(error, "CsvReader: delimiter is not a printable character");
        return false;
    }
    _splitter.set_delimiter((char *)&c);
    return true;
}

bool    CsvReader::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return true;
    }
    SIHD_LOG(error, "CsvReader: commentary is not a printable character");
    return false;
}

bool    CsvReader::open(const std::string & path)
{
    return _file.open(path, "r");
}

bool    CsvReader::is_open() const
{
    return _file.is_open();
}

bool    CsvReader::close()
{
    return _file.close();
}

void    CsvReader::_free_line()
{
    if (_line_ptr != nullptr)
    {
        free(_line_ptr);
        _line_ptr = nullptr;
        _line_size = 0;
    }
}

bool    CsvReader::read_next()
{
    if (_file.eof())
        return false;
    ssize_t ret;
    while ((ret = _file.read_line(&_line_ptr, &_line_size)) > 0)
    {
        if (_line_ptr[0] != _comment && _line_ptr[0] != '\n')
        {
            _line_ptr[ret - 1] = 0;
            break ;
        }
    }
    // ret < 0 can mean an end of file
    if (ret < 0 && errno > 0)
        SIHD_LOG(error, "CsvReader: read error: " << strerror(errno));
    return ret >= 0;
}

bool    CsvReader::get_read_data(char **data, size_t *size) const
{
    if (_line_ptr == nullptr)
        return false;
    *data = _line_ptr;
    *size = _line_size;
    return true;
}

bool    CsvReader::get_read_timestamp(time_t *nano_timestamp) const
{
    if (_line_ptr == nullptr)
        return false;
    int offset = _line_size > 0 && _quote > 0 && _line_ptr[0] == _quote ? 1 : 0;
    std::string str = _line_ptr + offset;
    return sihd::util::Str::to_long(str, (long *)nano_timestamp);
}

bool    CsvReader::get_read_data(std::vector<std::string> & values) const
{
    if (_line_ptr == nullptr)
        return false;
    values = _splitter.split(_line_ptr);
    return true;
}



}