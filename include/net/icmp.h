#pragma once

#include <net/ethernet.h>
#include <net/ipv4.h>
#include "types.h"


// #include <types.h>

#define ICMP_REPLY 0x00
#define ICMP_V4_ECHO 0x08

struct icmp_header
{
    u8 type;
    u8 code;
    u16 checksum;
    u16 id;
    u16 sequence;
} __attribute__((packed));

struct icmp_packet
{
    struct ether_header ether_header;
    struct ipv4_header ip_header;
    struct icmp_header icmp_header;
    u8 payload[];
} __attribute__((packed));

struct icmp_echo_reply
{
    u16 sequence;
    u8 ip[4];
};

typedef bool( *ICMP_ECHO_REPLY_CALLBACK)(struct icmp_echo_reply echo_reply);

void icmp_receive(u8 *packet, u16 len);
void icmp_send_echo_request(const u8 dest_ip[static 4], u16 sequence);