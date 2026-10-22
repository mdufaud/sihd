#ifndef __SIHD_CORE_RECORDS_HPP__
#define __SIHD_CORE_RECORDS_HPP__

#include <list>
#include <map>

#include <sihd/util/IArray.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::core
{

struct PlayableRecord
{
        std::string name;
        sihd::util::Timestamp timestamp {};
        sihd::util::IArrayShared value = nullptr;
};

using RecordedValue = std::pair<sihd::util::Timestamp, sihd::util::IArrayShared>;
using ListRecordedValues = std::list<RecordedValue>;
using MapListRecordedValues = std::map<std::string, ListRecordedValues>;
using SortedRecordedValues = std::multimap<sihd::util::Timestamp, PlayableRecord>;

} // namespace sihd::core

#endif