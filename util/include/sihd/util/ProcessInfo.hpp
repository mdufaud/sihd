#ifndef __SIHD_UTIL_PROCESSINFO_HPP__
#define __SIHD_UTIL_PROCESSINFO_HPP__

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class ProcessInfo
{
    public:
        ProcessInfo(int pid);
        // exact name match
        ProcessInfo(std::string_view name);
        ProcessInfo(const sihd::util::ProcessInfo & process_info);
        ~ProcessInfo();

        static std::vector<ProcessInfo> get_all_process_from_name(const std::string & regex);

        bool is_alive() const;

        int pid() const;
        const std::string & name() const;
        const std::string & cwd() const;
        const std::string & exe_path() const;
        const std::vector<std::string> & cmd_line() const;
        const std::vector<std::string> & env() const;
        Timestamp creation_time() const;

        bool write_into_stdin(ArrCharView view) const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::util

#endif
