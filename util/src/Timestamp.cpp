#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>

namespace sihd::util
{

Timestamp::Timestamp(time_t nano): _nano(nano)
{
}

Timestamp::Timestamp(std::chrono::nanoseconds duration): _nano(duration.count())
{
}

Timestamp::Timestamp(std::chrono::microseconds duration): _nano(time::duration<std::micro>(duration))
{
}

Timestamp::Timestamp(std::chrono::milliseconds duration): _nano(time::duration<std::milli>(duration))
{
}

Timestamp::Timestamp(std::chrono::seconds duration): _nano(time::duration<std::ratio<1>>(duration))
{
}

Timestamp::Timestamp(std::chrono::minutes duration): _nano(time::duration<std::ratio<60>>(duration))
{
}

Timestamp::Timestamp(std::chrono::hours duration): _nano(time::duration<std::ratio<3600>>(duration))
{
}

Timestamp::~Timestamp()
{
}

std::string     Timestamp::gmtime_str(bool total_parenthesis, bool nano_resolution)
{
    return Str::gmtime_str(_nano, total_parenthesis, nano_resolution);
}

std::string     Timestamp::localtime_str(bool total_parenthesis, bool nano_resolution)
{
    return Str::localtime_str(_nano, total_parenthesis, nano_resolution);
}


}