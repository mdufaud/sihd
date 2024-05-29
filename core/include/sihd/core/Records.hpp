#ifndef __SIHD_CORE_RECORDS_HPP__
#define __SIHD_CORE_RECORDS_HPP__

#include <list>
#include <map>

#include <sihd/util/IArray.hpp>

namespace sihd::core
{

struct PlayableRecord
{
        std::string name;
        time_t timestamp = 0;
        sihd::util::IArrayShared value = nullptr;
};

typedef std::pair<time_t, sihd::util::IArrayShared> RecordedValue;
typedef std::list<RecordedValue> ListRecordedValues;
typedef std::map<std::string, ListRecordedValues> MapListRecordedValues;
typedef std::multimap<time_t, PlayableRecord> SortedRecordedValues;

} // namespace sihd::core

#endif