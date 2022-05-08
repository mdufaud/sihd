#ifndef __SIHD_PCAP_PCAPUTILS_HPP__
# define __SIHD_PCAP_PCAPUTILS_HPP__

# include <sihd/util/platform.hpp>
# include <pcap.h>
# include <vector>
# include <string>

namespace sihd::pcap
{

class PcapUtils
{
    public:
        static bool init(unsigned int opts = PCAP_CHAR_ENC_UTF_8);

        static bool lookupnet(std::string_view dev, bpf_u_int32 *ip, bpf_u_int32 *mask);

        static std::string status_str(int code);
        static std::string version();

        static bool is_timestamp_type(int ts);
        static std::string timestamp_type_str(int ts);
        static std::string timestamp_type_desc(int dtl);
        static int timestamp_type(std::string_view dtl);

        static bool is_datalink(int dtl);
        static std::string datalink_str(int dtl);
        static std::string datalink_desc(int dtl);
        static int datalink(std::string_view dtl);

    protected:

    private:
        ~PcapUtils() {};

        static bool _is_init;
};

}

#endif