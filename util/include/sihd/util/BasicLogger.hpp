#ifndef __SIHD_UTIL_BASICLOGGER_HPP__
#define __SIHD_UTIL_BASICLOGGER_HPP__

#include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class BasicLogger: public ALogger
{
    public:
        BasicLogger(FILE *output = stderr, bool print_thread_id = false);
        virtual ~BasicLogger();

        virtual void log(const LogInfo & info, std::string_view msg) override;

        bool print_thread_id;

    private:
        FILE *_output;
};

} // namespace sihd::util

#endif