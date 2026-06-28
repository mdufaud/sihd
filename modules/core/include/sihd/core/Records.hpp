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
        sihd::util::Timestamp timestamp = 0;
        sihd::util::IArrayShared value = nullptr;
};

typedef std::pair<sihd::util::Timestamp, sihd::util::IArrayShared> RecordedValue;
typedef std::list<RecordedValue> ListRecordedValues;
typedef std::map<std::string, ListRecordedValues> MapListRecordedValues;
typedef std::multimap<sihd::util::Timestamp, PlayableRecord> SortedRecordedValues;

} // namespace sihd::core

#endif