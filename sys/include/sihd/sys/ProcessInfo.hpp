#ifndef __SIHD_SYS_PROCESSINFO_HPP__
#define __SIHD_SYS_PROCESSINFO_HPP__

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::sys
{

class ProcessInfo
{
    public:
        ProcessInfo(int pid);
        // exact name match
        ProcessInfo(std::string_view name);
        ProcessInfo(const ProcessInfo & process_info);
        ~ProcessInfo();

        static std::vector<ProcessInfo> get_all_process_from_name(const std::string & regex);

        bool is_alive() const;

        int pid() const;
        const std::string & name() const;
        const std::string & cwd() const;
        const std::string & exe_path() const;
        const std::vector<std::string> & cmd_line() const;
        const std::vector<std::string> & env() const;
        sihd::util::Timestamp creation_time() const;

        bool write_into_stdin(sihd::util::ArrCharView view) const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
