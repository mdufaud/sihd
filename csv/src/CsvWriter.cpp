#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvWriter)

SIHD_LOGGER;

CsvWriter::CsvWriter(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
    _delimiter = ',';
    _comment = '#';
    _line_feed = '\n';
    _col = 0;
    _row = 0;
    _max_col = 0;
    _begin_quote_c = -1;
    _end_quote_c = -1;
    _file.set_buffering_line();
    _file.set_buffer_size(4096);
    this->add_conf("quote", &CsvWriter::set_quote_value);
    this->add_conf("delimiter", &CsvWriter::set_delimiter);
    this->add_conf("comment", &CsvWriter::set_commentary);
}

CsvWriter::~CsvWriter()
{
}

bool    CsvWriter::set_quote_value(int c)
{
    _end_quote_c = sihd::util::str::closing_escape_of(c);
    if (_end_quote_c < 0)
    {
        _begin_quote_c = -1;
        SIHD_LOG(error, "CsvWriter: quote character '{}' is not supported", c);
        return false;
    }
    _begin_quote_c = c;
    return true;
}

bool    CsvWriter::set_delimiter(int c)
{
    if (std::isprint(c))
    {
        _delimiter = c;
        return true;
    }
    SIHD_LOG(error, "CsvWriter: delimiter is not a printable character");
    return false;
}

bool    CsvWriter::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return true;
    }
    SIHD_LOG(error, "CsvWriter: commentary is not a printable character");
    return false;
}

bool    CsvWriter::open(std::string_view path, bool append)
{
    _col = 0;
    _row = 0;
    _max_col = 0;
    if (_file.open(path, append ? "a" : "w"))
        return _file.buff_stream();
    return false;
}

bool    CsvWriter::is_open() const
{
    return _file.is_open();
}

bool    CsvWriter::close()
{
    return _file.close();
}

bool    CsvWriter::new_row()
{
    if (_file.write_char(_line_feed) == false)
    {
        SIHD_LOG(error, "CsvWriter: failed to write new line");
        return false;
    }
    _row += 1;
    _col = 0;
    return true;
}

ssize_t CsvWriter::write_commentary(std::string_view value)
{
    ssize_t ret = 0;
    if (_col > 0)
        ret = (ssize_t)this->new_row();
    ret += (ssize_t)_file.write_char(_comment);
    ret += _file.write(value);
    ret += (ssize_t)this->new_row();
    if (ret < (ssize_t)(value.size() + 2))
        SIHD_LOG(error, "CsvWriter: failed to write commentary");
    return ret;
}

ssize_t CsvWriter::write(const char *data, size_t size)
{
    ssize_t ret = 0;
    //,
    if (_col > 0)
        ret = (ssize_t)_file.write_char(_delimiter);
    //"
    if (_begin_quote_c > 0)
    {
        ret += _file.write_char(_begin_quote_c);
        ret += _file.write(data, size);
        ret += _file.write_char(_end_quote_c);
    }
    else
        ret += _file.write(data, size);
    if (ret < (ssize_t)size)
        SIHD_LOG_ERROR("CsvWriter: write failed '{}' < '{}'", ret, size);
    _col += ret;
    _max_col = std::max(_max_col, _col);
    return ret;
}

ssize_t CsvWriter::write(const char *data, size_t size, time_t nano_timestamp)
{
    return this->write(std::to_string(nano_timestamp))
        + this->write(data, size)
        + this->new_row();
}

ssize_t CsvWriter::write(std::string_view value)
{
    return this->write(value.data(), value.size());
}

ssize_t CsvWriter::write(std::string_view value, time_t nano_timestamp)
{
    return this->write(value.data(), value.size(), nano_timestamp);
}

ssize_t CsvWriter::write(const std::vector<std::string> & values, time_t nano_timestamp)
{
    return this->write(std::to_string(nano_timestamp)) + this->write(values);
}

ssize_t CsvWriter::write(const std::vector<std::string> & values)
{
    ssize_t ret = 0;
    ssize_t wrote;
    for (const std::string & value: values)
    {
        wrote = this->write(value);
        if (wrote < 0)
            break ;
        ret += wrote;
    }
    return ret;
}

ssize_t CsvWriter::write_row(const std::vector<std::string> & values)
{
    return this->write(values) + (ssize_t)this->new_row();
}

ssize_t CsvWriter::write_row(const std::vector<std::string> & values, time_t nano_timestamp)
{
    return this->write(std::to_string(nano_timestamp)) + this->write_row(values);
}

}
