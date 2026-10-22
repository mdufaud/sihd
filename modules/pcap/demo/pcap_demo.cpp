#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/pcap/PcapInterfaces.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/str.hpp>

#if defined(__SIHD_WINDOWS__)
// libwebsockets typedefs suseconds_t on windows, contrary to libpcap: this define prevents a redeclaration
# define LWS_HAVE_SUSECONDS_T
#endif

using namespace sihd::util;

namespace test::module
{

using namespace sihd::sys;
using namespace sihd::pcap;

SIHD_NEW_LOGGER("demo");

namespace
{

std::string interfaces_test()
{
    SIHD_LOG(info, "Net interfaces");
    std::string interface_to_sniff;
    PcapInterfaces ifaces;
    for (const auto & iface : ifaces.ifaces())
    {
        SIHD_LOG(info, "{}", iface.dump());
        if (iface.up() && !iface.loopback())
        {
            interface_to_sniff = iface.name();
        }
    }
    fmt::print("\n");
    return interface_to_sniff;
}

void sniffer_test(const std::string & interface_to_sniff)
{
    SIHD_LOG(info, "Sniffing on eth0");
    Sniffer pcap("pcap-sniffer");
    Handler<Sniffer *> obs([](Sniffer *obj) {
        constexpr size_t hexdump_cols = 20;
        SIHD_LOG(info, "Sniffed {} bytes", obj->array().size());
        const auto lines = str::hexdump_fmt(obj->array().buf(), obj->array().byte_size(), hexdump_cols);
        SIHD_COUT("{}\n", fmt::join(lines, "\n"));
    });
    if (pcap.open(interface_to_sniff).has_value() == false)
        return;
    pcap.add_observer(&obs);

    pcap.set_promiscuous(false);
    pcap.set_snaplen(2048);
    pcap.set_timeout(512);

    SIHD_LOG(info, "Activating packet capture");
    SIHD_UNEXPECTED_LOG(pcap.activate());
    if (pcap.is_active())
    {
        pcap.set_filter("portrange 0-2000");
        pcap.sniff();
        pcap.close();
    }
    SIHD_LOG(info, "Ending packet capture");
    fmt::print("\n");
}

} // namespace

} // namespace test::module

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "pcap_demo",
        .description = "Packet capture demo",
    });

    app.root().on_run([] {
        std::string interface_to_sniff = test::module::interfaces_test();
        test::module::sniffer_test(interface_to_sniff);
        if (build::is_windows)
            time::sleep(5);
    });

    return app.run(argc, argv);
}
