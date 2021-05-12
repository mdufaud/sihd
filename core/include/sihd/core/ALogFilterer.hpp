#ifndef __SIHD_CORE_ALOGFILTERER_HPP__
# define __SIHD_CORE_ALOGFILTERER_HPP__

# include <sihd/core/LogInfo.hpp>
# include <sihd/core/ILoggerFilter.hpp>
# include <list>

namespace sihd::core
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