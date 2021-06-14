#ifndef __SIHD_UTIL_BASICLOGGER_HPP__
# define __SIHD_UTIL_BASICLOGGER_HPP__

# include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class BasicLogger: public ALogger
{
    public:
        BasicLogger(FILE *output = stderr, bool print_thread_id = false);
        virtual ~BasicLogger();

        virtual void    log(const LogInfo & info, const char *msg) override;

        FILE    *output;
        bool    print_thread_id;
    private:
};

}

#endif 