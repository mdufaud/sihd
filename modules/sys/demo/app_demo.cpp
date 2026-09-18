#include <map>
#include <memory>
#include <string>

#include <fmt/format.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/Command.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

SIHD_NEW_LOGGER("app-demo");

namespace demo
{

class EchoService: public AService
{
    public:
        EchoService(sihd::sys::App & app, Command & parent, const std::string & name): _app(app), _name(name)
        {
            Command & cmd = parent.add_command(name, "service " + name);
            cmd.bind("greeting", _greeting, "greeting used by say");
            cmd.add_command("say", "say something through the service")
                .bind_positional("text", _text, "text to say")
                .on_run([this] { SIHD_LOG(info, "{}: {} {}", _name, _greeting, _text); });
            cmd.add_command("start", "start the service until a stop signal").on_run([this] {
                if (this->start() == false)
                    _app.exit(EXIT_FAILURE);
                SIHD_LOG(notice, "service {} started (Ctrl+C to stop)", _name);
                _app.wait_for_termination();
                this->stop();
            });
        }

        ~EchoService() override = default;

        bool is_running() const override { return _running; }

    protected:
        bool do_start() override
        {
            SIHD_LOG(notice, "{}: started", _name);
            _running = true;
            return true;
        }

        bool do_stop() override
        {
            SIHD_LOG(notice, "{}: stopped", _name);
            _running = false;
            return true;
        }

    private:
        sihd::sys::App & _app;
        std::string _name;
        std::string _greeting = "hello";
        std::string _text;
        bool _running = false;
};

} // namespace demo

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "app_demo",
        .description = "sihd application demo",
        .version = "1.0",
    });

    int serve_port = 4242;
    app.root()
        .add_command("serve", "serve forever")
        .bind("port", serve_port, "port to serve")
        .on_run([&serve_port, &app] {
            SIHD_LOG(notice, "serving on port {} (Ctrl+C to stop)", serve_port);
            app.wait_for_termination();
        });

    bool quick = false;
    app.root()
        .add_command("scan", "scan the network")
        .bind("quick", quick, "stop at the first result")
        .on_run([&quick] {
            SIHD_LOG(notice, "scanning network...{}", quick ? " (quick)" : "");
            for (int i = 1; i <= 3; ++i)
            {
                SIHD_LOG(info, "found host 192.168.1.{}", i);
                if (quick)
                    break;
            }
        });
    app.root().add_command("stats", "show statistics").on_run([] {
        SIHD_LOG(info, "no statistics collected in this demo");
    });
    app.root().get_command("stats")->add_command("memory", "memory statistics").on_run([] {
        SIHD_LOG(info, "current_rss: {}", str::bytes_str(sihd::sys::os::current_rss()));
    });

    std::map<std::string, std::unique_ptr<demo::EchoService>> services;
    Command & service_cmd = app.root().add_command("service", "manage services");

    std::string added_name;
    service_cmd.add_command("add", "create a service and its command branch")
        .bind_positional("name", added_name, "service name")
        .on_run([&] {
            if (added_name.empty() || added_name == "add" || added_name == "remove" || added_name == "list")
            {
                SIHD_LOG(error, "invalid service name '{}'", added_name);
                app.exit(EXIT_FAILURE);
            }
            if (services.contains(added_name))
            {
                SIHD_LOG(error, "service {} already exists", added_name);
                app.exit(EXIT_FAILURE);
            }
            services[added_name] = std::make_unique<demo::EchoService>(app, service_cmd, added_name);
            SIHD_LOG(notice, "service {} created, its branch is live:", added_name);
            fmt::print("{}\n", service_cmd.get_command(added_name)->help());
        });

    std::string removed_name;
    service_cmd.add_command("remove", "stop a service and remove its branch")
        .bind_positional("name", removed_name, "service name")
        .on_run([&] {
            auto it = services.find(removed_name);
            if (it == services.end())
            {
                SIHD_LOG(error, "unknown service {}", removed_name);
                app.exit(EXIT_FAILURE);
            }
            it->second->stop();
            // the branch must die with the service it captures
            service_cmd.remove_command(removed_name);
            services.erase(it);
            SIHD_LOG(notice, "service {} removed", removed_name);
        });

    service_cmd.add_command("list", "list live services").on_run([&] {
        if (services.empty())
            SIHD_LOG(notice, "no service - create one: {} service add toto", app.name());
        for (const auto & [name, service] : services)
            SIHD_LOG(info, "{} ({})", name, service->is_running() ? "running" : "stopped");
    });

    return app.run(argc, argv);
}
