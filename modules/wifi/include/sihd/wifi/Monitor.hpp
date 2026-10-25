#ifndef __SIHD_WIFI_MONITOR_HPP__
#define __SIHD_WIFI_MONITOR_HPP__

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/Duration.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/connection.hpp>

namespace sihd::wifi
{

class Monitor: public sihd::util::Observable<Monitor>,
               public sihd::util::IRunnable
{
    public:
        struct Event
        {
                enum class Type
                {
                    scan_done,
                    state_changed
                };

                Type type = Type::scan_done;
                // never read unless state_changed
                State state = State::disconnected;
        };

        Monitor();
        explicit Monitor(sihd::util::Duration run_timeout);
        virtual ~Monitor();

        // subscribes to the backend of the interface, failing like backend() for the same interface
        sihd::util::Result<void> open(const std::string & interface_name);

        // a negative timeout waits for an event
        void set_run_timeout(sihd::util::Duration timeout);

        // false when reading failed or nothing is open
        bool run() override;

        const std::vector<Event> & events() const;

    private:
        struct Impl;

        // nullopt once the timeout elapsed, the events one notification carries otherwise
        sihd::util::Result<std::optional<std::vector<Event>>> _receive(sihd::util::Duration timeout);
        // never publishes a state equal to the last one
        void _publish(const Event & event);

        std::unique_ptr<Impl> _impl;
        sihd::util::Duration _run_timeout = std::chrono::milliseconds(100);
        std::vector<Event> _events;
        std::optional<State> _last_state;
};

} // namespace sihd::wifi

#endif
