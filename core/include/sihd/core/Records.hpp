#ifndef __SIHD_CORE_RECORDS_HPP__
#define __SIHD_CORE_RECORDS_HPP__

#include <list>
#include <map>

#include <sihd/util/IArray.hpp>
#include <sihd/util/time.hpp>

namespace sihd::core
{

struct PlayableRecord
{
        std::string name;
        sihd::util::time::UnixTime timestamp = 0;
        sihd::util::IArrayShared value = nullptr;
};

typedef std::pair<sihd::util::time::UnixTime, sihd::util::IArrayShared> RecordedValue;
typedef std::list<RecordedValue> ListRecordedValues;
typedef std::map<std::string, ListRecordedValues> MapListRecordedValues;
typedef std::multimap<sihd::util::time::UnixTime, PlayableRecord> SortedRecordedValues;

} // namespace sihd::core

#endif