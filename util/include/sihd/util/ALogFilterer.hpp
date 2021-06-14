#ifndef __SIHD_UTIL_ALOGFILTERER_HPP__
# define __SIHD_UTIL_ALOGFILTERER_HPP__

# include <sihd/util/LogInfo.hpp>
# include <sihd/util/ILoggerFilter.hpp>
# include <list>

namespace sihd::util
{

class ALogFilterer
{
    public:
        ALogFilterer();
        virtual ~ALogFilterer();

        bool    has_filter(ILoggerFilter *filter);
        bool    add_filter(ILoggerFilter *filter);
        bool    remove_filter(ILoggerFilter *filter);
        void    delete_filters();
        bool    should_filter(const LogInfo & info, const char *msg);

    private:
        std::list<ILoggerFilter *>::iterator  _find(ILoggerFilter *filter);
        std::list<ILoggerFilter *>  _filters;
};

}

#endif 