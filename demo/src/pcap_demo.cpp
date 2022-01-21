#include <sihd/util/platform.hpp>

#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/Handler.hpp>

#include <sihd/pcap/Sniffer.hpp>
#include <sihd/pcap/PcapInterfaces.hpp>

#if defined(__SIHD_WINDOWS__)
// prevents error: previous declaration as 'typedef long int suseconds_t'
// windows libwebsockets - contrary to libpcap
// have a way to not typedef based on this define
# define LWS_HAVE_SUSECONDS_T
#endif

#include <unistd.h> // usleep

namespace test::module
{

using namespace sihd::util;
using namespace sihd::pcap;

NEW_LOGGER("test::module");


static std::string interfaces_test()
{
    LOG(info, "Net interfaces");
    std::string interface_to_sniff;
    PcapInterfaces ifaces;
    for (const auto & iface: ifaces.ifaces())
    {
        LOG(info, iface.dump());
        if (iface.up() && !iface.loopback())
        {
            interface_to_sniff = iface.name();
        }
    }
    std::cout << std::endl;
    return interface_to_sniff;
}

static void sniffer_test(const std::string & interface_to_sniff)
{
    LOG(info, "Sniffing on eth0");
    Sniffer pcap("pcap-sniffer");
    Handler<Sniffer *> obs([] (Sniffer *obj)
    {
        // TRACE(obj->array().hexdump());
        LOG(info, obj->array().size());
        std::cout << Str::hexdump_fmt(obj->array().cbuf(), obj->array().byte_size()) << std::endl;
    });
    pcap.add_observer(&obs);
    pcap.open(interface_to_sniff);
    if (pcap.is_open() == true)
    {
        // pcap.set_monitor(true);
        // pcap.set_immediate(true);
        pcap.set_promiscuous(false);
        pcap.set_snaplen(2048);
        pcap.set_timeout(512);
        LOG(info, "Activating packet capture");
        pcap.activate();
        if (pcap.is_active())
        {
            pcap.set_filter("portrange 0-2000");
            // sniff once
            pcap.sniff();
            usleep(10E3);
            pcap.close();
        }
    }
    LOG(info, "Ending packet capture");
    std::cout << std::endl;
}

} // end test::module

int main()
{
    sihd::util::Str::hexdump_cols = 20;
    sihd::util::LoggerManager::basic();
    std::string interface_to_sniff = test::module::interfaces_test();
    test::module::sniffer_test(interface_to_sniff);
    return 0;
}