#ifndef __SIHD_UTIL_ALOGFILTERER_HPP__
#define __SIHD_UTIL_ALOGFILTERER_HPP__

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
            std::lock_guard<std::mutex> l(_mutex);
            T *filtercast;
            bool found = false;
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
            }
            return found;
        }

        void delete_filters();
        bool should_filter(const LogInfo & info, std::string_view msg) const;

    private:
        std::list<ILoggerFilter *> _filters_lst;
        mutable std::mutex _mutex;
};

} // namespace sihd::util

#endif