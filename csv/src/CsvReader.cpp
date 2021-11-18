#include <sihd/csv/CsvReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvReader)

NEW_LOGGER("sihd::csv");

CsvReader::CsvReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

CsvReader::~CsvReader()
{
}

}