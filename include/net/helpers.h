#pragma once
#include "types.h"


u16 checksum(void *addr, int count, int start_sum);

unsigned int ip_to_int(const char ip[static 1]);
void int_to_ip(u32 ip_addr, char *result[static 16]);
u16 ntohs(u16 data);
u16 htons(u16 data);
u32 htonl(u32 data);
char *get_mac_address_string(u8 mac[static 6]);