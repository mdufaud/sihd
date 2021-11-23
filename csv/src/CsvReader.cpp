#include <sihd/csv/CsvReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Str.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvReader)

NEW_LOGGER("sihd::csv");

CsvReader::CsvReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
    _line_size = 0;
    _line_ptr = nullptr;
    _delimiter = ';';
    _comment = '#';
    _file.buffering_none();
    this->add_conf("delimiter", &CsvReader::set_delimiter);
    this->add_conf("comment", &CsvReader::set_commentary);
}

CsvReader::~CsvReader()
{
    this->_free_line();
}

bool    CsvReader::set_delimiter(int c)
{
    if (std::isprint(c))
    {
        _delimiter = c;
        return true;
    }
    LOG(error, "CsvReader: delimiter is not a printable character");
    return false;
}

bool    CsvReader::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return true;
    }
    LOG(error, "CsvReader: commentary is not a printable character");
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
        LOG(error, "CsvReader: read error: " << strerror(errno));
    return ret >= 0;
}

bool    CsvReader::get_values(std::vector<std::string> & values)
{
    if (_line_ptr == nullptr)
        return false;
    values = sihd::util::Str::split_escape(_line_ptr, (char *)(&_delimiter));
    return true;
}

}