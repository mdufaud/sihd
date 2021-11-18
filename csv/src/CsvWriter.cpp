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
}

CsvWriter::~CsvWriter()
{
}

}