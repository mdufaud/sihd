#ifndef __SIHD_NET_NETUTILS_HPP__
#define __SIHD_NET_NETUTILS_HPP__

#include <cstdint>

namespace sihd::net::utils
{

uint16_t checksum(uint16_t *addr, int len);

} // namespace sihd::net::utils

// Icmp structures for Windows

#if defined(__SIHD_WINDOWS__)

# include <winsock2.h>

# if !defined(__SIHD_WINDOWS_NET_IP_V4__)
#  define __SIHD_WINDOWS_NET_IP_V4__

/*
 * Structure of an internet header, naked of options.
 */
struct ip
{
#  if BYTE_ORDER == LITTLE_ENDIAN
        unsigned int ip_hl: 4; /* header length */
        unsigned int ip_v: 4;  /* version */
#  else
        unsigned int ip_v: 4;  /* version */
        unsigned int ip_hl: 4; /* header length */
#  endif
        uint8_t ip_tos;                /* type of service */
        unsigned short ip_len;         /* total length */
        unsigned short ip_id;          /* identification */
        unsigned short ip_off;         /* fragment offset field */
#  define IP_RF 0x8000                 /* reserved fragment flag */
#  define IP_DF 0x4000                 /* dont fragment flag */
#  define IP_MF 0x2000                 /* more fragments flag */
#  define IP_OFFMASK 0x1fff            /* mask for fragmenting bits */
        uint8_t ip_ttl;                /* time to live */
        uint8_t ip_p;                  /* protocol */
        unsigned short ip_sum;         /* checksum */
        struct in_addr ip_src, ip_dst; /* source and dest address */
};

# endif

# if !defined(__SIHD_WINDOWS_NET_ICMP_V4__)
#  define __SIHD_WINDOWS_NET_ICMP_V4__

/*
 * Internal of an ICMP Router Advertisement
 */
struct icmp_ra_addr
{
        uint32_t ira_addr;
        uint32_t ira_preference;
};

struct icmp
{
        uint8_t icmp_type;   /* type of message, see below */
        uint8_t icmp_code;   /* type sub code */
        uint16_t icmp_cksum; /* ones complement checksum of struct */
        union
        {
                unsigned char ih_pptr;    /* ICMP_PARAMPROB */
                struct in_addr ih_gwaddr; /* gateway address */
                struct ih_idseq           /* echo datagram */
                {
                        uint16_t icd_id;
                        uint16_t icd_seq;
                } ih_idseq;
                uint32_t ih_void;

                /* ICMP_UNREACH_NEEDFRAG -- Path MTU Discovery (RFC1191) */
                struct ih_pmtu
                {
                        uint16_t ipm_void;
                        uint16_t ipm_nextmtu;
                } ih_pmtu;

                struct ih_rtradv
                {
                        uint8_t irt_num_addrs;
                        uint8_t irt_wpa;
                        uint16_t irt_lifetime;
                } ih_rtradv;
        } icmp_hun;
#  define icmp_pptr icmp_hun.ih_pptr
#  define icmp_gwaddr icmp_hun.ih_gwaddr
#  define icmp_id icmp_hun.ih_idseq.icd_id
#  define icmp_seq icmp_hun.ih_idseq.icd_seq
#  define icmp_void icmp_hun.ih_void
#  define icmp_pmvoid icmp_hun.ih_pmtu.ipm_void
#  define icmp_nextmtu icmp_hun.ih_pmtu.ipm_nextmtu
#  define icmp_num_addrs icmp_hun.ih_rtradv.irt_num_addrs
#  define icmp_wpa icmp_hun.ih_rtradv.irt_wpa
#  define icmp_lifetime icmp_hun.ih_rtradv.irt_lifetime
        union
        {
                struct
                {
                        uint32_t its_otime;
                        uint32_t its_rtime;
                        uint32_t its_ttime;
                } id_ts;
                struct
                {
                        struct ip idi_ip;
                        /* options and then 64 bits of data */
                } id_ip;
                struct icmp_ra_addr id_radv;
                uint32_t id_mask;
                uint8_t id_data[1];
        } icmp_dun;
#  define icmp_otime icmp_dun.id_ts.its_otime
#  define icmp_rtime icmp_dun.id_ts.its_rtime
#  define icmp_ttime icmp_dun.id_ts.its_ttime
#  define icmp_ip icmp_dun.id_ip.idi_ip
#  define icmp_radv icmp_dun.id_radv
#  define icmp_mask icmp_dun.id_mask
#  define icmp_data icmp_dun.id_data
};

#  define ICMP_ECHOREPLY 0       /* Echo Reply			*/
#  define ICMP_DEST_UNREACH 3    /* Destination Unreachable	*/
#  define ICMP_SOURCE_QUENCH 4   /* Source Quench		*/
#  define ICMP_REDIRECT 5        /* Redirect (change route)	*/
#  define ICMP_ECHO 8            /* Echo Request			*/
#  define ICMP_TIME_EXCEEDED 11  /* Time Exceeded		*/
#  define ICMP_PARAMETERPROB 12  /* Parameter Problem		*/
#  define ICMP_TIMESTAMP 13      /* Timestamp Request		*/
#  define ICMP_TIMESTAMPREPLY 14 /* Timestamp Reply		*/
#  define ICMP_INFO_REQUEST 15   /* Information Request		*/
#  define ICMP_INFO_REPLY 16     /* Information Reply		*/
#  define ICMP_ADDRESS 17        /* Address Mask Request		*/
#  define ICMP_ADDRESSREPLY 18   /* Address Mask Reply		*/
#  define NR_ICMP_TYPES 18

/* Definition of type and code fields. */
/* defined above: ICMP_ECHOREPLY, ICMP_REDIRECT, ICMP_ECHO */
#  define ICMP_UNREACH 3        /* dest unreachable, codes: */
#  define ICMP_SOURCEQUENCH 4   /* packet lost, slow down */
#  define ICMP_ROUTERADVERT 9   /* router advertisement */
#  define ICMP_ROUTERSOLICIT 10 /* router solicitation */
#  define ICMP_TIMXCEED 11      /* time exceeded, code: */
#  define ICMP_PARAMPROB 12     /* ip header bad */
#  define ICMP_TSTAMP 13        /* timestamp request */
#  define ICMP_TSTAMPREPLY 14   /* timestamp reply */
#  define ICMP_IREQ 15          /* information request */
#  define ICMP_IREQREPLY 16     /* information reply */
#  define ICMP_MASKREQ 17       /* address mask request */
#  define ICMP_MASKREPLY 18     /* address mask reply */

#  define ICMP_MAXTYPE 18

/* UNREACH codes */
#  define ICMP_UNREACH_NET 0                /* bad net */
#  define ICMP_UNREACH_HOST 1               /* bad host */
#  define ICMP_UNREACH_PROTOCOL 2           /* bad protocol */
#  define ICMP_UNREACH_PORT 3               /* bad port */
#  define ICMP_UNREACH_NEEDFRAG 4           /* IP_DF caused drop */
#  define ICMP_UNREACH_SRCFAIL 5            /* src route failed */
#  define ICMP_UNREACH_NET_UNKNOWN 6        /* unknown net */
#  define ICMP_UNREACH_HOST_UNKNOWN 7       /* unknown host */
#  define ICMP_UNREACH_ISOLATED 8           /* src host isolated */
#  define ICMP_UNREACH_NET_PROHIB 9         /* net denied */
#  define ICMP_UNREACH_HOST_PROHIB 10       /* host denied */
#  define ICMP_UNREACH_TOSNET 11            /* bad tos for net */
#  define ICMP_UNREACH_TOSHOST 12           /* bad tos for host */
#  define ICMP_UNREACH_FILTER_PROHIB 13     /* admin prohib */
#  define ICMP_UNREACH_HOST_PRECEDENCE 14   /* host prec vio. */
#  define ICMP_UNREACH_PRECEDENCE_CUTOFF 15 /* prec cutoff */

/* REDIRECT codes */
#  define ICMP_REDIRECT_NET 0     /* for network */
#  define ICMP_REDIRECT_HOST 1    /* for host */
#  define ICMP_REDIRECT_TOSNET 2  /* for tos and net */
#  define ICMP_REDIRECT_TOSHOST 3 /* for tos and host */

/* TIMEXCEED codes */
#  define ICMP_TIMXCEED_INTRANS 0 /* ttl==0 in transit */
#  define ICMP_TIMXCEED_REASS 1   /* ttl==0 in reass */

#  define ICMP_MINLEN 8 /* abs minimum */

# endif

# if !defined(__SIHD_WINDOWS_NET_ICMP_V6__)
#  define __SIHD_WINDOWS_NET_ICMP_V6__

#  define ICMP6_FILTER 1

#  define ICMP6_FILTER_BLOCK 1
#  define ICMP6_FILTER_PASS 2
#  define ICMP6_FILTER_BLOCKOTHERS 3
#  define ICMP6_FILTER_PASSONLY 4

struct icmp6_filter
{
        uint32_t icmp6_filt[8];
};

struct icmp6_hdr
{
        uint8_t icmp6_type;   /* type field */
        uint8_t icmp6_code;   /* code field */
        uint16_t icmp6_cksum; /* checksum field */
        union
        {
                uint32_t icmp6_un_data32[1]; /* type-specific field */
                uint16_t icmp6_un_data16[2]; /* type-specific field */
                uint8_t icmp6_un_data8[4];   /* type-specific field */
        } icmp6_dataun;
};

#  define icmp6_data32 icmp6_dataun.icmp6_un_data32
#  define icmp6_data16 icmp6_dataun.icmp6_un_data16
#  define icmp6_data8 icmp6_dataun.icmp6_un_data8
#  define icmp6_pptr icmp6_data32[0]     /* parameter prob */
#  define icmp6_mtu icmp6_data32[0]      /* packet too big */
#  define icmp6_id icmp6_data16[0]       /* echo request/reply */
#  define icmp6_seq icmp6_data16[1]      /* echo request/reply */
#  define icmp6_maxdelay icmp6_data16[0] /* mcast group membership */

#  define ICMP6_DST_UNREACH 1
#  define ICMP6_PACKET_TOO_BIG 2
#  define ICMP6_TIME_EXCEEDED 3
#  define ICMP6_PARAM_PROB 4

#  define ICMP6_INFOMSG_MASK 0x80 /* all informational messages */

#  define ICMP6_ECHO_REQUEST 128
#  define ICMP6_ECHO_REPLY 129
#  define MLD_LISTENER_QUERY 130
#  define MLD_LISTENER_REPORT 131
#  define MLD_LISTENER_REDUCTION 132

#  define ICMP6_DST_UNREACH_NOROUTE 0     /* no route to destination */
#  define ICMP6_DST_UNREACH_ADMIN 1       /* communication with destination */
                                          /* administratively prohibited */
#  define ICMP6_DST_UNREACH_BEYONDSCOPE 2 /* beyond scope of source address */
#  define ICMP6_DST_UNREACH_ADDR 3        /* address unreachable */
#  define ICMP6_DST_UNREACH_NOPORT 4      /* bad port */

#  define ICMP6_TIME_EXCEED_TRANSIT 0    /* Hop Limit == 0 in transit */
#  define ICMP6_TIME_EXCEED_REASSEMBLY 1 /* Reassembly time out */

#  define ICMP6_PARAMPROB_HEADER 0     /* erroneous header field */
#  define ICMP6_PARAMPROB_NEXTHEADER 1 /* unrecognized Next Header */
#  define ICMP6_PARAMPROB_OPTION 2     /* unrecognized IPv6 option */

#  define ICMP6_FILTER_WILLPASS(type, filterp) ((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) == 0)

#  define ICMP6_FILTER_WILLBLOCK(type, filterp) ((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) != 0)

#  define ICMP6_FILTER_SETPASS(type, filterp) ((((filterp)->icmp6_filt[(type) >> 5]) &= ~(1 << ((type) & 31))))

#  define ICMP6_FILTER_SETBLOCK(type, filterp) ((((filterp)->icmp6_filt[(type) >> 5]) |= (1 << ((type) & 31))))

#  define ICMP6_FILTER_SETPASSALL(filterp) memset(filterp, 0, sizeof(struct icmp6_filter));

#  define ICMP6_FILTER_SETBLOCKALL(filterp) memset(filterp, 0xFF, sizeof(struct icmp6_filter));

# endif

#endif

#endif
