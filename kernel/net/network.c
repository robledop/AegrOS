#include <e1000.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/helpers.h>
#include <net/icmp.h>
#include <net/ipv4.h>
#include <net/network.h>
#include <net/udp.h>

#include "defs.h"
#include "string.h"

bool network_ready       = false;
u8 *my_ip_address   = nullptr;
u8 *default_gateway = nullptr;
u8 *subnet_mask     = nullptr;
u32 *dns_servers;

static u8 *mac = nullptr;

struct ether_type {
    u16 ether_type;
    char *name;
};

struct ether_type ether_types[] = {
    {ETHERTYPE_PUP,      "Xerox PUP"               },
    {ETHERTYPE_SPRITE,   "Sprite"                  },
    {ETHERTYPE_IP,       "IPv4"                    },
    {ETHERTYPE_ARP,      "ARP"                     },
    {ETHERTYPE_REVARP,   "Reverse ARP"             },
    {ETHERTYPE_AT,       "AppleTalk protocol"      },
    {ETHERTYPE_AARP,     "AppleTalk ARP"           },
    {ETHERTYPE_VLAN,     "IEEE 802.1Q VLAN tagging"},
    {ETHERTYPE_IPX,      "IPX"                     },
    {ETHERTYPE_IPV6,     "IPv6"                    },
    {ETHERTYPE_LOOPBACK, "Loopback"                }
};

void network_set_state(bool state)
{
    network_ready = state;
}

bool network_is_ready()
{
    return network_ready;
}

void network_set_dns_servers(u32 dns_servers_p[static 1], u32 dns_server_count)
{
    dns_servers = (u32 *)kmalloc(sizeof(u32) * dns_server_count);
    memcpy(dns_servers, dns_servers_p, sizeof(u32) * dns_server_count);
}

void network_set_my_ip_address(const u8 ip[static 4])
{
    if (my_ip_address == nullptr) {
        my_ip_address = (u8 *)kmalloc(4);
    }
    memcpy(my_ip_address, ip, 4);
}

void network_set_subnet_mask(const u8 ip[static 4])
{
    if (subnet_mask == nullptr) {
        subnet_mask = (u8 *)kmalloc(4);
    }
    memcpy(subnet_mask, ip, 4);
}

void network_set_default_gateway(const u8 ip[static 4])
{
    if (default_gateway == nullptr) {
        default_gateway = (u8 *)kmalloc(4);
    }
    memcpy(default_gateway, ip, 4);
}

u8 *network_get_my_ip_address()
{
    return my_ip_address;
}

u8 *network_get_my_mac_address()
{
    return mac;
}

const char *find_ether_type(const u16 ether_type)
{
    for (size_t i = 0; i < sizeof(ether_types) / sizeof(struct ether_type); i++) {
        if (ether_types[i].ether_type == ether_type) {
            return ether_types[i].name;
        }
    }
    return "Unknown";
}

void network_set_mac(const u8 mac_addr[static 6])
{
    if (mac == nullptr) {
        mac = (u8 *)kmalloc(6);
    }
    memcpy(mac, mac_addr, 6);
}

void network_receive(u8 *packet, const u16 len)
{
    const struct ether_header *ether_header = (struct ether_header *)packet;
    const u16 ether_type               = ntohs(ether_header->ether_type);

    switch (ether_type) {
    case ETHERTYPE_ARP:
        arp_receive(packet);
        break;
    case ETHERTYPE_IP:
        auto ipv4_header       = (struct ipv4_header *)(packet + sizeof(struct ether_header));
        const u8 protocol = ipv4_header->protocol;
        switch (protocol) {
        case IP_PROTOCOL_ICMP:
            if (my_ip_address && network_compare_ip_addresses(ipv4_header->dest_ip, my_ip_address)) {
                icmp_receive(packet, len);
            }
            break;
        case IP_PROTOCOL_TCP:
            break;
        case IP_PROTOCOL_UDP:
            auto udp_header = (struct udp_header *)(packet + sizeof(struct ether_header) + sizeof(struct ipv4_header));

            if (udp_header->dest_port == htons(DHCP_SOURCE_PORT)) {
                dhcp_receive(packet);
            }
            break;
        default:
        }
        break;
    default:
    }
}

int network_send_packet(const void *data, const u16 len)
{
    return e1000_send_packet(data, len);
}

bool network_compare_ip_addresses(const u8 ip1[static 4], const u8 ip2[static 4])
{
    if (ip1 == ip2) {
        return true;
    }
    return memcmp(ip1, ip2, 4) == 0;
}

bool network_compare_mac_addresses(const u8 mac1[static 6], const u8 mac2[static 6])
{
    if (mac1 == mac2) {
        return true;
    }
    return memcmp(mac1, mac2, 6) == 0;
}
