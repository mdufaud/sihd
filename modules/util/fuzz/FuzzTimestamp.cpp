#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <locale>
#include <string>

#include <fmt/format.h>

#include <sihd/util/Timestamp.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr int64_t nanos_per_second = 1'000'000'000;
constexpr int64_t seconds_per_day = 86'400;
constexpr size_t max_text_size = 64;
constexpr size_t round_trip_formats = 3;
constexpr std::array<const char *, 9> formats = {Timestamp::default_format,
                                                 Timestamp::default_day_format,
                                                 "%Y-%m-%d %H:%M:%S",
                                                 "%FT%T%z",
                                                 "%d/%m/%Y %H:%M",
                                                 "%H:%M:%S",
                                                 "%a %b %d %Y",
                                                 "%j %Y",
                                                 "%y-%m-%d %I:%M %p"};

// civil calendar to days since 1970-01-01, valid for years >= 0
int64_t days_from_civil(int64_t year, int64_t month, int64_t day)
{
    const int64_t shifted_year = month <= 2 ? year - 1 : year;
    const int64_t era = shifted_year / 400;
    const int64_t year_of_era = shifted_year - era * 400;
    const int64_t day_of_year = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const int64_t day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + day_of_era - 719468;
}

int64_t floor_to_second(int64_t nanos)
{
    const int64_t remainder = nanos % nanos_per_second;
    return remainder < 0 ? nanos - remainder - nanos_per_second : nanos - remainder;
}

void fuzz_raw(FuzzedDataProvider & provider)
{
    const size_t index = provider.ConsumeIntegralInRange<size_t>(0, formats.size() - 1);
    const std::string text = provider.ConsumeBytesAsString(max_text_size);
    const std::string format = formats[index];

    const Result<Timestamp> parsed = Timestamp::from_str(text, format, std::locale::classic());
    if (!parsed.has_value() || index >= round_trip_formats)
        return;
    // the second below the minimum timestamp cannot be represented
    if (parsed->get() < std::numeric_limits<int64_t>::min() + nanos_per_second)
        return;

    const std::string printed = parsed->format(format, std::locale::classic());
    const Result<Timestamp> reparsed = Timestamp::from_str(printed, format, std::locale::classic());
    test::fuzz_check(reparsed.has_value() && reparsed->get() == floor_to_second(parsed->get()),
                     "a parsed date prints to text that parses to the same second");
}

void fuzz_calendar(FuzzedDataProvider & provider)
{
    const int year = provider.ConsumeIntegralInRange<int>(1700, 2200);
    const int month = provider.ConsumeIntegralInRange<int>(1, 12);
    const int day = provider.ConsumeIntegralInRange<int>(1, 28);
    const bool with_time = provider.ConsumeBool();
    const int hour = with_time ? provider.ConsumeIntegralInRange<int>(0, 23) : 0;
    const int minute = with_time ? provider.ConsumeIntegralInRange<int>(0, 59) : 0;
    const int second = with_time ? provider.ConsumeIntegralInRange<int>(0, 59) : 0;

    char text[32];
    if (with_time)
        std::snprintf(text, sizeof(text), "%04d/%02d/%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    else
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d", year, month, day);
    const std::string format = with_time ? Timestamp::default_format : Timestamp::default_day_format;

    const Result<Timestamp> parsed = Timestamp::from_str(text, format, std::locale::classic());
    test::fuzz_check(parsed.has_value(), "a canonical date parses");
    const int64_t seconds = days_from_civil(year, month, day) * seconds_per_day + hour * 3600 + minute * 60 + second;
    test::fuzz_check(parsed->get() == seconds * nanos_per_second, "a canonical date is the expected instant");

    const Calendar calendar = parsed->calendar();
    test::fuzz_check(calendar.year == year && calendar.month == month && calendar.day == day,
                     "calendar gives back the parsed date");
    const Clocktime clocktime = parsed->clocktime();
    test::fuzz_check(clocktime.hour == hour && clocktime.minute == minute && clocktime.second == second
                         && clocktime.millisecond == 0,
                     "clocktime gives back the parsed time");
    test::fuzz_check(parsed->format(format, std::locale::classic()) == text, "format gives back the canonical text");
}

void fuzz_range_edge(FuzzedDataProvider & provider)
{
    constexpr std::array<uint32_t, 6> edge_fractions = {0, 145224191, 145224192, 854775807, 854775808, 999999999};
    const bool upper = provider.ConsumeBool();
    const int year = upper ? 2262 : 1677;
    const int month = upper ? 4 : 9;
    const int day = upper ? 11 : 21;
    const int hour = upper ? 23 : 0;
    const int minute = upper ? 47 : 12;
    const int second = (upper ? 14 : 41) + provider.ConsumeIntegralInRange<int>(0, 5);
    const uint32_t fraction = provider.ConsumeBool() ? provider.PickValueInArray(edge_fractions)
                                                     : provider.ConsumeIntegralInRange<uint32_t>(0, 999999999);

    const std::string date = fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", year, month, day, hour, minute, second)
                             + (fraction == 0 ? "" : fmt::format(".{:09}", fraction));

    const __int128 seconds = days_from_civil(year, month, day) * seconds_per_day + hour * 3600 + minute * 60 + second;
    const __int128 nanos = seconds * nanos_per_second + fraction;
    const bool representable = nanos >= std::numeric_limits<int64_t>::min()
                               && nanos <= std::numeric_limits<int64_t>::max();

    const Result<Timestamp> parsed = Timestamp::from_str(date, "%Y-%m-%d %H:%M:%S", std::locale::classic());
    test::fuzz_check(parsed.has_value() == representable, "a date parses exactly when its nanoseconds fit");
    if (representable)
        test::fuzz_check(parsed->get() == static_cast<int64_t>(nanos),
                         "a date at the range edge is the expected instant");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_raw(provider);
            break;
        case 1:
            fuzz_calendar(provider);
            break;
        default:
            fuzz_range_edge(provider);
            break;
    }
    return 0;
}
