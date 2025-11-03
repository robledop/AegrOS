#pragma once

#include "types.h"

void network_receive(u8 *packet, u16 len);
int network_send_packet(const void *data, u16 len);
void network_set_mac(const u8 mac_addr[static 6]);
u8 *network_get_my_ip_address(void);
bool network_compare_ip_addresses(const u8 ip1[static 4], const u8 ip2[static 4]);
bool network_compare_mac_addresses(const u8 mac1[static 6], const u8 mac2[static 6]);
u8 *network_get_my_mac_address(void);
bool network_is_ready(void);
void network_set_dns_servers(u32 dns_servers_p[static 1], u32 dns_server_count);
void network_set_my_ip_address(const u8 ip[static 4]);
void network_set_subnet_mask(const u8 ip[static 4]);
void network_set_default_gateway(const u8 ip[static 4]);
void network_set_state(bool state);