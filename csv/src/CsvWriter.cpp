#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvWriter)

LOGGER;

CsvWriter::CsvWriter(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent), _file_ptr(nullptr)
{
    _delimiter = 'c';
    _comment = '#';
    _line_feed = '\n';
    _col = 0;
    _row = 0;
    _max_col = 0;
    this->add_conf("delimiter", &CsvWriter::set_delimiter);
    this->add_conf("comment", &CsvWriter::set_commentary);
}

CsvWriter::~CsvWriter()
{
    this->close();
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
    this->close();
    _col = 0;
    _row = 0;
    _max_col = 0;
    _file_ptr = fopen(path.c_str(), append ? "a" : "w");
    if (_file_ptr == nullptr)
        LOG(error, "CsvWriter: could not open file: " << path);
    return _file_ptr != nullptr;
}

bool    CsvWriter::is_open() const
{
    return _file_ptr != nullptr;
}

bool    CsvWriter::close()
{
    if (_file_ptr != nullptr)
    {
        if (fclose(_file_ptr) != 0)
        {
            LOG(error, "CsvWriter: could not close file: " << strerror(errno));
            return false;
        }
        _file_ptr = nullptr;
    }
    return true;
}

bool    CsvWriter::write_commentary(const std::string & value)
{
    bool ret = true;
    if (_col > 0)
        ret = this->new_row();
    ret = ret && fputc(_comment, _file_ptr) == _comment;
    ret = ret && fputs(value.c_str(), _file_ptr) == (int)value.size();
    ret = ret && this->new_row();
    if (!ret)
        LOG(error, "CsvWriter: failed to write commentary");
    return ret;
}

bool    CsvWriter::new_row()
{
    if (fputc(_line_feed, _file_ptr) != _line_feed)
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
        ret = fputc(_delimiter, _file_ptr) == _delimiter;
    ret = ret && fputs(value.c_str(), _file_ptr) == (int)value.size();        
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