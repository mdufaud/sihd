#ifndef __SIHD_UTIL_STAT_HPP__
#define __SIHD_UTIL_STAT_HPP__

#include <sys/types.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace sihd::util
{

template <typename SampleType>
struct Stat
{
        size_t samples = 0;
        SampleType min = SampleType {};
        SampleType sum = SampleType {};
        SampleType square = SampleType {};
        SampleType max = SampleType {};

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

        SampleType average() const { return samples == 0 ? SampleType {} : static_cast<SampleType>(sum / samples); }

        SampleType variance() const
        {
            if (samples == 0)
                return SampleType {};
            const SampleType average = this->average();
            return (square / samples) - (average * average);
        }

        SampleType standard_deviation() const { return static_cast<SampleType>(::sqrt(this->variance())); }

        // Percentiles via inverse-normal (z-score) approximation.
        // Accurate for symmetric, unimodal distributions; degrades on heavy tails.
        SampleType median() const { return this->average(); }

        SampleType p95() const { return static_cast<SampleType>(this->average() + 1.645 * this->standard_deviation()); }

        SampleType p99() const { return static_cast<SampleType>(this->average() + 2.326 * this->standard_deviation()); }
};

namespace detail
{

// P-Square algorithm (Jain & Chlamtac, 1985): online streaming percentile estimator.
// Uses 5 markers; no memory allocation; accurate on arbitrary continuous distributions.
struct PSquareEstimator
{
        explicit PSquareEstimator(double quantile): p(quantile) {}

        void add(double x)
        {
            if (count < 5)
            {
                init[count++] = x;
                if (count == 5)
                {
                    std::sort(init.begin(), init.end());
                    q = init;
                }
                return;
            }

            ++count;

            int k;
            if (x < q[0])
            {
                q[0] = x;
                k = 0;
            }
            else if (x < q[1])
                k = 0;
            else if (x < q[2])
                k = 1;
            else if (x < q[3])
                k = 2;
            else if (x < q[4])
                k = 3;
            else
            {
                q[4] = x;
                k = 3;
            }

            for (int i = k + 1; i < 5; ++i)
                ++n[i];

            // desired positions — computed dynamically from total count
            const double N = static_cast<double>(count);
            const std::array<double, 5> nd = {
                1.0,
                1.0 + (N - 1.0) * p / 2.0,
                1.0 + (N - 1.0) * p,
                1.0 + (N - 1.0) * (1.0 + p) / 2.0,
                N,
            };

            for (int i = 1; i <= 3; ++i)
            {
                const double d = nd[i] - static_cast<double>(n[i]);
                if ((d >= 1.0 && n[i + 1] - n[i] > 1) || (d <= -1.0 && n[i - 1] - n[i] < -1))
                {
                    const int sign = (d > 0) ? 1 : -1;
                    const double qi = q[i];
                    const double qip1 = q[i + 1]; // always i+1, not i+sign
                    const double qim1 = q[i - 1]; // always i-1, not i-sign
                    const int ni = n[i];
                    const int nip1 = n[i + 1];
                    const int nim1 = n[i - 1];

                    // parabolic interpolation (standard P-Square formula)
                    const double qnew = qi
                                        + static_cast<double>(sign) / static_cast<double>(nip1 - nim1)
                                              * (static_cast<double>(ni - nim1 + sign) * (qip1 - qi)
                                                     / static_cast<double>(nip1 - ni)
                                                 + static_cast<double>(nip1 - ni - sign) * (qi - qim1)
                                                       / static_cast<double>(ni - nim1));

                    if (qim1 < qnew && qnew < qip1)
                        q[i] = qnew;
                    else
                        q[i] = qi
                               + static_cast<double>(sign) * (q[i + sign] - qi) / static_cast<double>(n[i + sign] - ni);

                    n[i] += sign;
                }
            }
        }

        double estimate() const
        {
            if (count == 0)
                return 0.0;
            if (count < 5)
            {
                std::array<double, 5> sorted = init;
                for (size_t i = 1; i < count; ++i)
                {
                    const double key = sorted[i];
                    size_t j = i;
                    while (j > 0 && sorted[j - 1] > key)
                    {
                        sorted[j] = sorted[j - 1];
                        --j;
                    }
                    sorted[j] = key;
                }
                const size_t idx = std::min(static_cast<size_t>(p * static_cast<double>(count - 1)),
                                            static_cast<size_t>(count - 1));
                return sorted[idx];
            }
            return q[2];
        }

        void clear()
        {
            count = 0;
            n = {1, 2, 3, 4, 5};
            q = {};
            init = {};
        }

    private:
        double p;
        size_t count = 0;
        std::array<int, 5> n = {1, 2, 3, 4, 5};
        std::array<double, 5> q = {};
        std::array<double, 5> init = {};
};

} // namespace detail

// Percentiles via P-Square streaming algorithm (Jain & Chlamtac, 1985).
// Accurate on asymmetric and heavy-tailed distributions (e.g. network latency).
// Extra memory: 3 estimators × ~200 bytes = ~600 bytes.
template <typename SampleType>
struct PSquareStat: public Stat<SampleType>
{
        PSquareStat(): _p50(0.50), _p95(0.95), _p99(0.99) {}

        void add_sample(const SampleType & sample)
        {
            Stat<SampleType>::add_sample(sample);
            const double x = static_cast<double>(sample);
            _p50.add(x);
            _p95.add(x);
            _p99.add(x);
        }

        void clear()
        {
            Stat<SampleType>::clear();
            _p50.clear();
            _p95.clear();
            _p99.clear();
        }

        SampleType median() const { return static_cast<SampleType>(_p50.estimate()); }
        SampleType p95() const { return static_cast<SampleType>(_p95.estimate()); }
        SampleType p99() const { return static_cast<SampleType>(_p99.estimate()); }

    private:
        detail::PSquareEstimator _p50;
        detail::PSquareEstimator _p95;
        detail::PSquareEstimator _p99;
};

} // namespace sihd::util

#endif
