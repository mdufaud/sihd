#ifndef __SIHD_UTIL_LOGGERMANAGER_HPP__
# define __SIHD_UTIL_LOGGERMANAGER_HPP__

# include <sihd/util/ALogger.hpp>
# include <sihd/util/ALogFilterer.hpp>
# include <sihd/util/LogInfo.hpp>
# include <list>
# include <mutex>

namespace sihd::util
{

class LoggerManager: public ALogFilterer
{
    public:

        LoggerManager();
        virtual ~LoggerManager();

        bool has_logger(ALogger *logger);
        bool add_logger(ALogger *logger);
        bool remove_logger(ALogger *logger);
        void remove_loggers();

        template <typename T>
        bool remove_logger_type()
        {
            T *logcast;
            bool found = false;
            auto it = _loggers_lst.begin();
            while (it != _loggers_lst.end())
            {
                logcast = dynamic_cast<T *>(*it);
                if (logcast != nullptr)
                {
                    delete logcast;
                    it = _loggers_lst.erase(it);
                    found = true;
                }
            }
            return found;
        }

        void log(const std::string & src, LogLevel level, const std::string_view & msg);

        static LoggerManager _g_singleton;
        static LoggerManager *get();

        static void basic(FILE *output = stderr, bool print_thread_id = false);
        static void console();
        static bool add(ALogger *logger);
        static bool remove(ALogger *logger);
        static bool filter(ILoggerFilter *filter);
        static bool unfilter(ILoggerFilter *filter);
        static void clear_loggers();
        static void clear_filters();

    private:
        std::list<ALogger *>::iterator _find(ALogger *logger);
        std::list<ALogger *> _loggers_lst;
        std::mutex _mutex;
};

}

#endif