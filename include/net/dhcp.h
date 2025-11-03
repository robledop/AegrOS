#pragma once

#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <stddef.h>
#include "types.h"

#define DHCP_CHADDR_LEN 6
#define DHCP_SNAME_LEN 64
#define DHCP_FILE_LEN 128
#define DHCP_OPTIONS_LEN 128


#define DHCP_OP_DISCOVER 0x01
#define DHCP_OP_REQUEST 0x01
#define DHCP_OP_OFFER 0x02
#define DHCP_SOURCE_PORT 68
#define DHCP_DEST_PORT 67

#define DHCP_HTYPE_ETH 0x01
#define DHCP_HLEN_ETH 0x06

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_FLAG_BROADCAST 0x8000
#define DHCP_FLAG_UNICAST 0x0000

#define DHCP_OPTION_MESSAGE_TYPE 53
#define DHCP_OPTION_PARAMETER_REQUEST 55
#define DHCP_OPTION_MESSAGE_TYPE_LEN 1

#define DHCP_MESSAGE_TYPE_DISCOVER 1
#define DHCP_MESSAGE_TYPE_OFFER 2
#define DHCP_MESSAGE_TYPE_REQUEST 3
#define DHCP_MESSAGE_TYPE_DECLINE 4
#define DHCP_MESSAGE_TYPE_ACK 5
#define DHCP_MESSAGE_TYPE_NAK 6
#define DHCP_MESSAGE_TYPE_RELEASE 7
#define DHCP_MESSAGE_TYPE_INFORM 8

#define DHCP_OPT_PAD 0
#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_ROUTER 3
#define DHCP_OPT_DNS 6
#define DHCP_OPT_HOST_NAME 12
#define DHCP_OPT_REQUESTED_IP_ADDR 50
#define DHCP_OPT_LEASE_TIME 51
#define DHCP_OPT_DHCP_MESSAGE_TYPE 53
#define DHCP_OPT_SERVER_ID 54
#define DHCP_OPT_PARAMETER_REQUEST 55
#define DHCP_OPT_CLIENT_ID 61
#define DHCP_OPT_END 255

struct dhcp_header {
    u8 op;
    u8 htype;
    u8 hlen;
    u8 hops;
    u32 xid;
    u16 secs;
    u16 flags;
    u8 ciaddr[4];               // Client IP address (only filled if client is already bound)
    u8 yiaddr[4];               // 'Your' (client) IP address
    u8 siaddr[4];               // IP address of next server to use in bootstrap
    u8 giaddr[4];               // Relay agent IP address
    u8 chaddr[DHCP_CHADDR_LEN]; // Client hardware address
    u8 reserved[10];
    u8 sname[DHCP_SNAME_LEN];
    u8 file[DHCP_FILE_LEN];
    u32 magic;
    u8 options[DHCP_OPTIONS_LEN];
} __attribute__((packed));

struct dhcp_packet {
    struct ether_header eth;
    struct ipv4_header ip;
    struct udp_header udp;
    struct dhcp_header dhcp;
} __attribute__((packed));

void dhcp_send_discover(u8 mac[static 6]);
void dhcp_send_request(u8 mac[static 6], u8 ip[static 4], u8 server_ip[static 4]);
u32 dhcp_options_get_ip_option(const u8 options[static DHCP_OPTIONS_LEN], int option);
int dhcp_options_get_dns_servers(const u8 options[static DHCP_OPTIONS_LEN],
                                         u32 dns_servers[static 1], size_t *dns_server_count);
void dhcp_receive(u8 *packet);
