#ifndef __SIHD_CORE_BASICLOGGER_HPP__
# define __SIHD_CORE_BASICLOGGER_HPP__

# include <sihd/core/ALogger.hpp>

namespace sihd::core
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