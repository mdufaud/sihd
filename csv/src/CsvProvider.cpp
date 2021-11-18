#include <sihd/csv/CsvProvider.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::csv
{

SIHD_UTIL_REGISTER_FACTORY(CsvProvider)

LOGGER;

CsvProvider::CsvProvider(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

CsvProvider::~CsvProvider()
{
}

}