#include <sihd/csv/CsvRecorder.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/str.hpp>

namespace sihd::csv
{

using namespace sihd::util;
using sihd::core::Channel;

SIHD_REGISTER_FACTORY(CsvRecorder)

SIHD_LOGGER;

namespace
{

std::string resolve_date(const std::string & path)
{
    return str::replace(path, "{date}", SystemClock {}.now().local_format("%Y-%m-%d_%H-%M-%S"));
}

// bytes and chars are hex: a raw byte could be a delimiter or a line feed
std::string values_of(const IArray & array)
{
    const Type type = array.data_type();
    if (type == TYPE_BYTE || type == TYPE_UBYTE || type == TYPE_CHAR)
        return array.hexdump(' ');
    return array.str(' ');
}

} // namespace

CsvRecorder::CsvRecorder(const std::string & name, Node *parent): sihd::core::ACoreObject(name, parent)
{
    this->add_conf("path", &CsvRecorder::set_path);
}

CsvRecorder::~CsvRecorder()
{
    if (this->is_running())
        this->stop();
}

bool CsvRecorder::set_path(const std::string & path)
{
    if (path.empty())
        return false;
    _path = path;
    return true;
}

bool CsvRecorder::is_running() const
{
    std::lock_guard lock(_writer_mutex);
    return _writer.is_open();
}

void CsvRecorder::handle(const std::string & name, const Channel *channel)
{
    const std::vector<std::string> row = {std::to_string(channel->timestamp().nanoseconds()),
                                          name,
                                          values_of(*channel->array())};
    std::lock_guard lock(_writer_mutex);
    if (_writer.write_row(row) < 0)
        SIHD_LOG(error, "CsvRecorder: {} could not record {}", this->name(), name);
}

bool CsvRecorder::do_start()
{
    if (_path.empty())
    {
        SIHD_LOG(error, "CsvRecorder: {} needs a path", this->name());
        return false;
    }
    std::lock_guard lock(_writer_mutex);
    if (SIHD_UNEXPECTED_LOG(_writer.open(resolve_date(_path))))
        return false;
    if (_writer.write_row(std::vector<std::string> {"timestamp", "channel", "value"}) < 0)
    {
        _writer.close();
        return false;
    }
    return true;
}

bool CsvRecorder::do_stop()
{
    std::lock_guard lock(_writer_mutex);
    return _writer.close();
}

} // namespace sihd::csv
