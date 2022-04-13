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

        static std::string get_status(int code);
        static std::string version();

        static bool is_timestamp_type(int ts);
        static std::string timestamp_type_to_string(int ts);
        static std::string timestamp_type_to_desc(int dtl);
        static int string_to_timestamp_type(std::string_view dtl);

        static bool is_datalink(int dtl);
        static std::string datalink_to_string(int dtl);
        static std::string datalink_to_desc(int dtl);
        static int string_to_datalink(std::string_view dtl);

    protected:

    private:
        static bool _is_init;

        PcapUtils() {};
        ~PcapUtils() {};
};

}

#endif