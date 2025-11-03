#pragma once
#include <net/ethernet.h>
#include "types.h"

#define ARP_REQUEST 1
#define ARP_REPLY 2

#define ARP_CACHE_SIZE 256
#define ARP_CACHE_TIMEOUT 60'000 // jiffies

struct arp_header
{
    u16 hw_type;
    u16 protocol_type;
    u8 hw_addr_len;
    u8 protocol_addr_len;
    u16 opcode;
    u8 sender_hw_addr[6];
    u8 sender_protocol_addr[4];
    u8 target_hw_addr[6];
    u8 target_protocol_addr[4];
};

struct arp_packet
{
    struct ether_header ether_header;
    struct arp_header arp_packet;
} __attribute__((packed));

struct arp_cache_entry
{
    u8 ip[4];
    u8 mac[6];
    u32 timestamp;
};

struct arp_cache_entry arp_cache_find(const u8 ip[static 4]);
void arp_receive(u8 *packet);
void arp_send_reply(u8 *packet);
void arp_send_request(const u8 dest_ip[static 4]);
void arp_init(void);