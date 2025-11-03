#pragma once

#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17
#include "types.h"

struct ipv4_header
{
    // 4 bits IHL (Internet Header Length)
    u8 ihl : 4;
    // 4 bits version (always 4),
    u8 version : 4;
    // 6 bits Differentiated Services Code Point (DSCP)
    // 2 bits Explicit Congestion Notification (ECN)
    u8 dscp_ecn;
    // Total length of the entire packet
    u16 total_length;
    u16 identification;
    // 3 bits flags, 13 bits fragment offset
    u16 flags_fragment_offset;
    // Time to live
    u8 ttl;
    // Protocol (TCP, UDP, ICMP, etc)
    u8 protocol;
    u16 header_checksum;
    u8 source_ip[4];
    u8 dest_ip[4];
} __attribute__((packed));