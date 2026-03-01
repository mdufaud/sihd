#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include <sihd/csv/CsvReader.hpp>

namespace sihd::csv
{

SIHD_NEW_LOGGER("sihd::csv");

using namespace sihd::util;

namespace
{

int count_unescaped_quotes(std::string_view view)
{
    int idx = 0;
    int count = 0;

    while (view.size() > 0)
    {
        idx = str::find_char_not_escaped(view, '"', '"');
        if (idx < 0)
            break;
        view = view.substr(idx + 1);
        ++count;
    }

    return count;
}

} // namespace

CsvReader::CsvReader()
{
    this->_reset_line();

    _comment = '#';

    _splitter.set_delimiter(",");
    _splitter.set_empty_delimitations(true);
    _splitter.set_open_escape_sequences("\"");

    _line_reader.set_read_buffsize(4096);
    _line_reader.set_delimiter_in_line(true);
}

CsvReader::CsvReader(std::string_view path): CsvReader()
{
    this->open(path);
}

CsvReader::~CsvReader() = default;

bool CsvReader::set_delimiter(int c)
{
    if (std::isprint(c) == 0)
    {
        SIHD_LOG(error, "CsvReader: delimiter is not a printable character");
        return false;
    }
    _delimiter = c;
    _splitter.set_delimiter((char *)&_delimiter);
    return true;
}

bool CsvReader::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return true;
    }
    SIHD_LOG(error, "CsvReader: commentary is not a printable character");
    return false;
}

void CsvReader::set_timestamp_col(size_t n)
{
    _timestamp_col = n;
}

void CsvReader::set_timestamp_format(std::string format)
{
    _timestamp_fmt = std::move(format);
}

bool CsvReader::open(std::string_view path)
{
    this->_reset_line();
    return _line_reader.open(path);
}

bool CsvReader::is_open() const
{
    return _line_reader.is_open();
}

bool CsvReader::close()
{
    this->_reset_line();
    return _line_reader.close();
}

void CsvReader::_reset_line()
{
    _has_data = false;
    _line.clear();
    _csv_cols.clear();
}

bool CsvReader::read_next()
{
    this->_reset_line();
    while (_line_reader.read_next())
    {
        ArrCharView view;
        _line_reader.get_read_data(view);

        // always empty lines
        if (view.empty())
            continue;

        const bool searching_for_end_quote = _line.empty() == false;
        const bool is_commentary = !searching_for_end_quote && view[0] == _comment;
        const bool is_unecessary_spaces = !searching_for_end_quote && str::is_all_spaces(view);

        // skip spaces or linefeed only if we are not searching for an end quote
        if (is_commentary || is_unecessary_spaces)
            continue;

        _line += view.data();

        const bool number_of_quotes_are_odd = count_unescaped_quotes(view) % 2;
        const bool quotes_are_even
            = searching_for_end_quote ? number_of_quotes_are_odd : !number_of_quotes_are_odd;

        if (quotes_are_even)
        {
            // remove last linefeed
            if (!_line.empty())
                _line.resize(_line.size() - 1);
            _has_data = true;
            return true;
        }
    }
    return false;
}

bool CsvReader::get_read_data(sihd::util::ArrCharView & view) const
{
    if (_has_data == false)
        return false;
    view = _line;
    return true;
}

bool CsvReader::get_read_timestamp(time_t *nano_timestamp) const
{
    if (_has_data == false || _timestamp_col < 0)
        return false;

    const auto & columns = this->columns();

    if ((int)columns.size() < _timestamp_col)
    {
        return false;
    }

    const auto & time_str = columns.at(_timestamp_col);

    if (_timestamp_fmt.empty())
    {
        return sihd::util::str::to_long(time_str, (long *)nano_timestamp);
    }
    else
    {
        auto opt_timestamp = Timestamp::from_str(time_str, _timestamp_fmt);
        if (opt_timestamp)
        {
            *nano_timestamp = opt_timestamp->nanoseconds();
            return true;
        }
        else
        {
            SIHD_LOG(error,
                     "CsvReader: cannot process timestamp value '{}' with format '{}'",
                     _timestamp_fmt,
                     time_str);
        }
    }

    return false;
}

const std::vector<std::string> & CsvReader::columns() const
{
    if (_has_data && _csv_cols.empty())
    {
        _csv_cols = _splitter.split(_line);
        for (auto & col : _csv_cols)
        {
            col = str::remove_escape_char(col, '"');
        }
    }
    return _csv_cols;
}

} // namespace sihd::csv
