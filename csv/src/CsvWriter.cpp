#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>

namespace sihd::csv
{

using namespace sihd::util;

SIHD_LOGGER;

CsvWriter::CsvWriter()
{
    _delimiter = ',';
    _comment = '#';
    _line_feed = '\n';
    _col = 0;
    _row = 0;
    _max_col = 0;
    _file.set_buffering_line();
    _file.set_buffer_size(4096);
}

CsvWriter::CsvWriter(std::string_view path, bool append): CsvWriter()
{
    this->open(path, append);
}

CsvWriter::~CsvWriter() = default;

bool CsvWriter::set_delimiter(int c)
{
    if (std::isprint(c))
    {
        _delimiter = c;
        return true;
    }
    SIHD_LOG(error, "CsvWriter: delimiter is not a printable character");
    return false;
}

bool CsvWriter::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return true;
    }
    SIHD_LOG(error, "CsvWriter: commentary is not a printable character");
    return false;
}

bool CsvWriter::open(std::string_view path, bool append)
{
    _col = 0;
    _row = 0;
    _max_col = 0;
    if (_file.open(path, append ? "a" : "w"))
        return _file.buff_stream();
    return false;
}

bool CsvWriter::is_open() const
{
    return _file.is_open();
}

bool CsvWriter::close()
{
    return _file.close();
}

bool CsvWriter::new_row()
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

ssize_t CsvWriter::write_commentary(std::string_view comment)
{
    ssize_t ret = 0;
    if (_col > 0)
        ret = (ssize_t)this->new_row();
    ret += (ssize_t)_file.write_char(_comment);
    ret += _file.write(comment);
    ret += (ssize_t)this->new_row();
    if (ret < (ssize_t)(comment.size() + 2))
        SIHD_LOG_ERROR("CsvWriter: commentary write failed '{}' < '{}'", ret, comment.size());
    return ret;
}

ssize_t CsvWriter::write(sihd::util::ArrCharView view)
{
    ssize_t ret = 0;
    //,
    if (_col > 0)
        ret = (ssize_t)_file.write_char(_delimiter);
    ret += _file.write(view);
    if (ret < (ssize_t)view.size())
        SIHD_LOG_ERROR("CsvWriter: write failed '{}' < '{}'", ret, view.size());
    _col += ret;
    _max_col = std::max(_max_col, _col);
    return ret;
}

ssize_t CsvWriter::write_row(sihd::util::ArrCharView view)
{
    return this->write(view) + (ssize_t)this->new_row();
}

ssize_t CsvWriter::write(const std::vector<std::string> & values)
{
    ssize_t ret = 0;
    ssize_t wrote;
    for (const std::string & value : values)
    {
        wrote = this->write(value);
        if (wrote < 0)
            break;
        ret += wrote;
    }
    return ret;
}

ssize_t CsvWriter::write_row(const std::vector<std::string> & values)
{
    return this->write(values) + (ssize_t)this->new_row();
}

} // namespace sihd::csv
