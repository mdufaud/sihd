#include <fmt/format.h>

#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::csv
{

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
    SIHD_UNEXPECTED_LOG(_file.set_buffer_size(4096));
}

CsvWriter::CsvWriter(std::string_view path, bool append): CsvWriter()
{
    SIHD_UNEXPECTED_LOG(this->open(path, append));
}

CsvWriter::~CsvWriter() = default;

std::expected<void, Error> CsvWriter::set_delimiter(int c)
{
    if (std::isprint(c))
    {
        _delimiter = c;
        return {};
    }
    return std::unexpected(Error(invalid_argument, "delimiter is not a printable character"));
}

std::expected<void, Error> CsvWriter::set_commentary(int c)
{
    if (std::isprint(c))
    {
        _comment = c;
        return {};
    }
    return std::unexpected(Error(invalid_argument, "commentary is not a printable character"));
}

std::expected<void, Error> CsvWriter::open(std::string_view path, bool append)
{
    _col = 0;
    _row = 0;
    _max_col = 0;
    // binary mode: avoid Windows text-mode CRLF translation (consistent LF output)
    auto opened = _file.open(path, append ? "ab" : "wb");
    if (!opened)
        return opened;
    auto buffered = _file.buff_stream();
    if (!buffered)
        return std::unexpected(Error(io_error, "could not buffer stream of '{}'", path));
    return {};
}

bool CsvWriter::is_open() const
{
    return _file.is_open();
}

bool CsvWriter::close()
{
    return !SIHD_UNEXPECTED_LOG(_file.close());
}

std::expected<void, Error> CsvWriter::new_row()
{
    auto fed = _file.write_char(_line_feed);
    SIHD_UNEXPECTED_RETURN_CTX(fed, "writing new row");
    _row += 1;
    _col = 0;
    return {};
}

ssize_t CsvWriter::write_commentary(std::string_view comment)
{
    if (_col > 0)
    {
        if (SIHD_UNEXPECTED_LOG(this->new_row()))
            return -1;
    }
    auto fed = _file.write_char(_comment);
    if (!fed)
    {
        SIHD_LOG(error, "CsvWriter: commentary write failed for comment char");
        return -1;
    }
    const auto wrote = _file.write(comment);
    if (!wrote || *wrote < comment.size())
    {
        SIHD_LOG(error, "CsvWriter: commentary write failed '{}' < '{}'", wrote.value_or(0), comment.size());
        return -1;
    }
    if (SIHD_UNEXPECTED_LOG(this->new_row()))
        return -1;
    // 1 for comment char + comment size + 1 for newline
    return 1 + (ssize_t)*wrote + 1;
}

ssize_t CsvWriter::write(sihd::util::ArrCharView view)
{
    ssize_t ret = 0;
    //,
    if (_col > 0)
    {
        auto fed = _file.write_char(_delimiter);
        if (!fed)
        {
            SIHD_LOG(error, "CsvWriter: write failed for delimiter char");
            return -1;
        }
        ret += 1;
    }
    const auto wrote = _file.write(view);
    if (!wrote || *wrote < view.size())
    {
        SIHD_LOG(error, "CsvWriter: write failed '{}' < '{}'", wrote.value_or(0), view.size());
        return -1;
    }
    ret += (ssize_t)*wrote;
    _col += 1;
    _max_col = std::max(_max_col, _col);
    return ret;
}

ssize_t CsvWriter::write_row(sihd::util::ArrCharView view)
{
    const ssize_t wrote = this->write(view);
    if (wrote < 0)
        return wrote;
    if (SIHD_UNEXPECTED_LOG(this->new_row()))
        return -1;
    return wrote + 1;
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
    const ssize_t wrote = this->write(values);
    if (wrote < 0)
        return wrote;
    if (this->new_row().has_value() == false)
        return -1;
    return wrote + 1;
}

} // namespace sihd::csv
