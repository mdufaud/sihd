#include <fmt/format.h>

#include <sihd/util/platform.hpp>

#include <sihd/util/File.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

#include <sihd/pcap/PcapInterfaces.hpp>
#include <sihd/pcap/Sniffer.hpp>

#if defined(__SIHD_WINDOWS__)
// prevents error: previous declaration as 'typedef long int suseconds_t'
// windows libwebsockets - contrary to libpcap
// have a way to not typedef based on this define
# define LWS_HAVE_SUSECONDS_T
#endif

namespace test::module
{

using namespace sihd::util;
using namespace sihd::pcap;

SIHD_NEW_LOGGER("demo");

static std::string interfaces_test()
{
    SIHD_LOG(info, "Net interfaces");
    std::string interface_to_sniff;
    PcapInterfaces ifaces;
    for (const auto & iface : ifaces.ifaces())
    {
        SIHD_LOG(info, iface.dump());
        if (iface.up() && !iface.loopback())
        {
            interface_to_sniff = iface.name();
        }
    }
    fmt::print("\n");
    return interface_to_sniff;
}

static void sniffer_test(const std::string & interface_to_sniff)
{
    SIHD_LOG(info, "Sniffing on eth0");
    Sniffer pcap("pcap-sniffer");
    Handler<Sniffer *> obs([](Sniffer *obj) {
        constexpr size_t hexdump_cols = 20;
        SIHD_LOG(info, "Sniffed {} bytes", obj->array().size());
        const auto lines = str::hexdump_fmt(obj->array().buf(), obj->array().byte_size(), hexdump_cols);
        SIHD_COUT("{}\n", fmt::join(lines, "\n"));
    });
    if (!pcap.open(interface_to_sniff))
        return;
    pcap.add_observer(&obs);
    // pcap.set_monitor(true);
    // pcap.set_immediate(true);
    pcap.set_promiscuous(false);
    pcap.set_snaplen(2048);
    pcap.set_timeout(512);
    SIHD_LOG(info, "Activating packet capture");
    pcap.activate();
    if (pcap.is_active())
    {
        pcap.set_filter("portrange 0-2000");
        // sniff once
        pcap.sniff();
        pcap.close();
    }
    SIHD_LOG(info, "Ending packet capture");
    fmt::print("\n");
}

} // namespace test::module

int main()
{
    sihd::util::LoggerManager::basic();

    std::string interface_to_sniff = test::module::interfaces_test();
    test::module::sniffer_test(interface_to_sniff);
    if (sihd::util::os::is_windows)
        sihd::util::time::sleep(5);

    return 0;
}
