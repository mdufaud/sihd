#ifndef __SIHD_UTIL_ALOGFILTERER_HPP__
#define __SIHD_UTIL_ALOGFILTERER_HPP__

#include <atomic>
#include <list>
#include <mutex>

#include <sihd/util/ILoggerFilter.hpp>
#include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

class ALogFilterer
{
    public:
        ALogFilterer();
        virtual ~ALogFilterer();

        bool has_filter(ILoggerFilter *filter) const;
        bool add_filter(ILoggerFilter *filter);
        bool remove_filter(ILoggerFilter *filter);

        template <typename T>
        bool remove_filter_type()
        {
            bool found = false;
            {
                std::lock_guard<std::mutex> l(_filters_mutex);
                T *filtercast;
                auto it = _filters_lst.begin();
                while (it != _filters_lst.end())
                {
                    filtercast = dynamic_cast<T *>(*it);
                    if (filtercast != nullptr)
                    {
                        delete filtercast;
                        it = _filters_lst.erase(it);
                        found = true;
                    }
                    else
                    {
                        ++it;
                    }
                }
                _filters_count.store(_filters_lst.size(), std::memory_order_relaxed);
            }
            return found;
        }

        void delete_filters();

        // phase 1: true when a filter drops for sure without the formatted message
        bool should_filter(const LogInfo & info) const;
        // both phases: true when any filter drops
        bool should_filter(const LogInfo & info, std::string_view msg) const;

    private:
        std::list<ILoggerFilter *> _filters_lst;
        // lock-bypass hint: never decides filtering, only skips the mutex on empty lists
        std::atomic<size_t> _filters_count {0};
        mutable std::mutex _filters_mutex;
};

} // namespace sihd::util

#endif
