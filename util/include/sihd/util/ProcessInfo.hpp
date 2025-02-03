#ifndef __SIHD_UTIL_PROCESSINFO_HPP__
#define __SIHD_UTIL_PROCESSINFO_HPP__

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>

namespace sihd::util
{

class ProcessInfo
{
    public:
        ProcessInfo(int pid);
        ProcessInfo(std::string_view name);
        virtual ~ProcessInfo();

        bool reload();
        bool is_alive() const;

        int pid() const;
        const std::string & name() const;
        const std::string & cwd() const;
        const std::string & exe_path() const;
        const std::vector<std::string> & cmd_line() const;
        const std::vector<std::string> & env() const;

        bool write_into_stdin(ArrCharView view) const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::util

#endif
