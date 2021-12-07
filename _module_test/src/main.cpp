#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/pcap/PcapInterfaces.hpp>

#include <unistd.h> // usleep

namespace test::module
{

using namespace sihd::util;
using namespace sihd::pcap;

NEW_LOGGER("test::module");

static void node_test()
{
    Node node("root");
    LOG(info, "Node root name: " << node.get_name());
    LOG(info, "Executable path: " << OS::get_executable_path());
    std::cout << std::endl;
}

static void interfaces_test()
{
    LOG(info, "Net interfaces");
    PcapInterfaces ifaces;
    for (const auto & iface: ifaces.ifaces())
    {
        LOG(info, iface.dump());
    }
    std::cout << std::endl;
}

static void sniffer_test()
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
    pcap.open("eth0");
    // pcap.set_monitor(true);
    // pcap.set_immediate(true);
    pcap.set_promiscuous(false);
    pcap.set_snaplen(2048);
    pcap.set_timeout(512);
    pcap.activate();
    if (pcap.is_active())
    {
        pcap.set_filter("portrange 0-2000");
        // sniff once
        pcap.sniff();
        usleep(10E3);
        pcap.close();
    }
    std::cout << std::endl;
}

}

int main()
{
    sihd::util::Str::hexdump_cols = 20;
    sihd::util::LoggerManager::basic();
    test::module::node_test();
    test::module::interfaces_test();
    test::module::sniffer_test();
    return 0;
}