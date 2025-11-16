#pragma once

#include <pci.h>
#include <types.h>

// AHCI register layout definitions based on the AHCI specification, section 3.3.
struct ahci_port
{
    u32 clb;      // 0x00, command list base address
    u32 clbu;     // 0x04, command list base address upper 32 bits
    u32 fb;       // 0x08, FIS base address
    u32 fbu;      // 0x0C, FIS base address upper 32 bits
    u32 is;       // 0x10, interrupt status
    u32 ie;       // 0x14, interrupt enable
    u32 cmd;      // 0x18, command and status
    u32 reserved; // 0x1C
    u32 tfd;      // 0x20, task file data
    u32 sig;      // 0x24, signature
    u32 ssts;     // 0x28, SATA status (SCR0)
    u32 sctl;     // 0x2C, SATA control (SCR2)
    u32 serr;     // 0x30, SATA error (SCR1)
    u32 sact;     // 0x34, SATA active (SCR3)
    u32 ci;       // 0x38, command issue
    u32 sntf;     // 0x3C, SATA notification (SCR4)
    u32 fbs;      // 0x40, FIS-based switch control
    u32 devslp;   // 0x44, device sleep
    u32 reserved2[10];
    u32 vendor[4];
} __attribute__((packed));

struct ahci_memory
{
    u32 cap;     // 0x00, host capability
    u32 ghc;     // 0x04, global host control
    u32 is;      // 0x08, interrupt status
    u32 pi;      // 0x0C, ports implemented
    u32 vs;      // 0x10, version
    u32 ccc_ctl; // 0x14, command completion coalescing control
    u32 ccc_pts; // 0x18, command completion coalescing ports
    u32 em_loc;  // 0x1C, enclosure management location
    u32 em_ctl;  // 0x20, enclosure management control
    u32 cap2;    // 0x24, host capabilities extended
    u32 bohc;    // 0x28, BIOS/OS handoff control and status
    u8 reserved[0xA0 - 0x2C];
    u8 vendor[0x100 - 0xA0];
    struct ahci_port ports[32];
} __attribute__((packed));

void ahci_init(struct pci_device device);
bool ahci_port_ready(void);
int ahci_read(u64 lba, u32 sector_count, void *buffer);
int ahci_write(u64 lba, u32 sector_count, const void *buffer);

#define AHCI_SECTOR_SIZE 512u
