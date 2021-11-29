#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvWriter)

LOGGER;

CsvWriter::CsvWriter(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
    _delimiter = ',';
    _comment = '#';
    _line_feed = '\n';
    _col = 0;
    _row = 0;
    _max_col = 0;
    _file.buffering_line();
    _file.set_bufsize(1024);
    this->add_conf("delimiter", &CsvWriter::set_delimiter);
    this->add_conf("comment", &CsvWriter::set_commentary);
}

CsvWriter::~CsvWriter()
{
}

bool    CsvWriter::set_delimiter(int c)
{
    if (std::isprint(c))
    {
        _delimiter = c;
        return true;
    }
    LOG(error, "CsvWriter: delimiter is not a printable character");
    return false;
}

bool    CsvWriter::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return true;
    }
    LOG(error, "CsvWriter: commentary is not a printable character");
    return false;
}

bool    CsvWriter::open(const std::string & path, bool append)
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

bool    CsvWriter::write_commentary(const std::string & value)
{
    bool ret = true;
    if (_col > 0)
        ret = this->new_row();
    ret = ret && _file.write_char(_comment);
    ret = ret && _file.write(value) == (ssize_t)value.size();
    ret = ret && this->new_row();
    if (!ret)
        LOG(error, "CsvWriter: failed to write commentary");
    return ret;
}

bool    CsvWriter::new_row()
{
    if (_file.write_char(_line_feed) == false)
    {
        LOG(error, "CsvWriter: failed to write new line");
        return false;
    }
    _row += 1;
    _col = 0;
    return true;
}

bool    CsvWriter::write(const std::string & value)
{
    bool ret = true;
    if (_col > 0)
        ret = _file.write_char(_delimiter);
    ret = ret && _file.write(value) == (ssize_t)value.size();
    _col += (int)ret;
    _max_col = std::max(_max_col, _col);
    if (!ret)
        LOG(error, "CsvWriter: failed to write value");
    return ret;
}

bool    CsvWriter::write(const std::vector<std::string> & values)
{
    for (const std::string & value: values)
    {
        if (this->write(value) == false)
            return false;
    }
    return true;
}

bool    CsvWriter::write_row(const std::vector<std::string> & values)
{
    return this->write(values) && this->new_row();
}

}