#ifndef __SIHD_UTIL_LOGINFO_HPP__
#define __SIHD_UTIL_LOGINFO_HPP__

#include <ctime>

#include <string>
#include <string_view>

#include <sihd/util/Timestamp.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util
{

enum class LogLevel
{
    none = 1000,
    emergency = 0, // A panic condition
    alert = 1,     // A condition that should be corrected immediately, such as a corrupted system database
    critical = 2,  // Hard device errors
    error = 3,     //  Errors that are fatal to the operation, but not the service or application (can't open a required
                   //  file, missing data, etc.) Solving these types of errors will usually require user intervention.
    warning = 4, // Anything that can potentially cause application oddities, but for which the system is automatically
                 // recovering from (e.g., retrying an operation, missing secondary data, etc.)
    notice = 5,  // Conditions that are not error conditions, but that may require special handling
    info = 6,    // Confirmation that the program is working as expected
    debug = 7    // Messages that contain information normally of use only when debugging a program
};

class LogInfo
{
    public:
        LogInfo(const std::string & source, LogLevel level);
        ~LogInfo();

        std::string_view source;
        LogLevel level;
        const char *strlevel;
        pthread_t thread_id;
        std::string thread_id_str;
        std::string_view thread_name;
        struct timespec timespec;

        static const char *level_str(LogLevel level);
        static LogLevel level_from_str(std::string_view level);
        Timestamp timestamp() const;
};

} // namespace sihd::util

#endif
