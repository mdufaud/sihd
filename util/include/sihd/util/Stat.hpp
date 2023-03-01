#ifndef __SIHD_UTIL_STAT_HPP__
#define __SIHD_UTIL_STAT_HPP__

#include <cmath>

#include <sys/types.h>

namespace sihd::util
{

template <typename SampleType>
struct Stat
{
        size_t samples = 0;
        SampleType min = {};
        SampleType sum = {};
        SampleType square = {};
        SampleType max = {};

        void add_sample(const SampleType & sample)
        {
            if (samples == 0)
            {
                min = sample;
                max = sample;
            }
            else
            {
                min = std::min(min, sample);
                max = std::max(max, sample);
            }

            sum += sample;
            square += (sample * sample);

            ++samples;
        }

        void clear()
        {
            samples = 0;
            min = SampleType {};
            sum = SampleType {};
            square = SampleType {};
            max = SampleType {};
        }

        SampleType average() const { return sum / samples; }

        SampleType variance() const
        {
            const SampleType average = this->average();
            return (square / samples) - (average * average);
        }

        SampleType standard_deviation() const { return ::sqrt(this->variance()); }
};

} // namespace sihd::util

#endif
