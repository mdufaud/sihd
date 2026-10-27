#ifndef __SIHD_SYS_DESKTOPNOTIFIER_HPP__
#define __SIHD_SYS_DESKTOPNOTIFIER_HPP__

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

class DesktopNotifier
{
    public:
        enum class Urgency
        {
            low,
            normal,
            critical
        };

        struct Notification
        {
                std::string summary;
                std::string body;
                std::string icon;
                Urgency urgency = Urgency::normal;
                std::optional<sihd::util::Duration> timeout;
        };

        static constexpr bool supported = (sihd::util::build::is_linux && !sihd::util::build::is_android
                                           && !sihd::util::build::is_emscripten)
                                          || sihd::util::build::is_windows;

        DesktopNotifier();
        DesktopNotifier(DesktopNotifier && other);
        DesktopNotifier & operator=(DesktopNotifier && other);
        ~DesktopNotifier();

        DesktopNotifier(const DesktopNotifier &) = delete;
        DesktopNotifier & operator=(const DesktopNotifier &) = delete;

        void set_app_name(const std::string & name);
        // required on windows (AppUserModelID)
        void set_app_id(const std::string & id);

        sihd::util::Result<uint32_t> notify(const Notification & notification);
        sihd::util::Result<void> close(uint32_t id);

    private:
        static sihd::util::Result<void> check_timeout(const std::optional<sihd::util::Duration> & timeout);

        struct Impl;

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
