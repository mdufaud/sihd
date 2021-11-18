#include <sihd/csv/CsvRecorder.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvRecorder)

LOGGER;

CsvRecorder::CsvRecorder(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

CsvRecorder::~CsvRecorder()
{
}

}