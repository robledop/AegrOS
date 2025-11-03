#pragma once
#include "types.h"

struct udp_header
{
    u16 src_port;
    u16 dest_port;
    u16 len;
    u16 checksum;
} __attribute__((packed));

struct udp_pseudo_header
{
    u8 src_ip[4];
    u8 dest_ip[4];
    u8 zero;        // Always 0
    u8 protocol;    // Protocol number (UDP is 17)
    u16 udp_length; // Length of UDP header + data
};