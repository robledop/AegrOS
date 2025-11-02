#include "e1000.h"
#include "defs.h"
#include "printf.h"
#include "io.h"
#include "x86.h"

#define IRQ0 0x20

void e1000_receive();
bool e1000_start();
void e1000_linkup();
void e1000_receive_packets();

static u8 bar_type;                                        // Type of BAR0
static u16 io_base;                                        // IO Base Address
static u64 mem_base;                                       // MMIO Base Address
static bool eeprom_exists;                                 // A flag indicating if eeprom exists
static u8 mac[6];                                          // A buffer for storing the mack address
static struct e1000_rx_desc *rx_descs[E1000_RX_RING_SIZE]; // Receive Descriptor Buffers
static struct e1000_tx_desc *tx_descs[E1000_TX_RING_SIZE]; // Transmit Descriptor Buffers
static u16 rx_cur;                                         // Current Receive Descriptor Buffer
static u16 tx_cur;                                         // Current Transmit Descriptor Buffer
static struct pci_device pci_device;


/**
 * @brief Write a 32-bit value to an e1000 register via MMIO or IO space.
 */
void e1000_write_command(const u16 p_address, const u32 p_value)
{
    if (bar_type == PCI_BAR_MEM) {
        write32(mem_base + p_address, p_value);
    } else {
        outl(io_base, p_address);
        outl(io_base + 4, p_value);
    }
}

/**
 * @brief Read a 32-bit value from an e1000 register via MMIO or IO space.
 */
u32 e1000_read_command(const u16 p_address)
{
    if (bar_type == PCI_BAR_MEM) {
        return read32(mem_base + p_address);
    }

    outl(io_base, p_address);
    return inl(io_base + 4);
}

/**
 * @brief Detect whether the controller has an EEPROM attached.
 *
 * @return true if an EEPROM is present, false otherwise.
 */
bool e1000_detect_eeprom()
{
    u32 val = 0;
    e1000_write_command(REG_EERD, 0x1);

    for (int i = 0; i < 1000 && !eeprom_exists; i++) {
        val = e1000_read_command(REG_EERD);
        if (val & 0x10) {
            eeprom_exists = true;
        } else {
            eeprom_exists = false;
        }
    }
    return eeprom_exists;
}

/**
 * @brief Read a 16-bit word from the controller's EEPROM.
 *
 * @param addr EEPROM word address.
 * @return Value read from the EEPROM.
 */
u32 e1000_eeprom_read(const u8 addr)
{
    u16 data = 0;
    u32 tmp  = 0;
    if (eeprom_exists) {
        e1000_write_command(REG_EERD, (1) | ((u32)(addr) << 8));
        while (!((tmp = e1000_read_command(REG_EERD)) & (1 << 4)));
    } else {
        e1000_write_command(REG_EERD, (1) | ((u32)(addr) << 2));
        while (!((tmp = e1000_read_command(REG_EERD)) & (1 << 1)));
    }
    data = (u16)((tmp >> 16) & 0xFFFF);
    return data;
}

/**
 * @brief Populate the MAC address from EEPROM or MMIO registers.
 *
 * @return true if a MAC address was successfully read.
 */
bool e1000_read_mac_address()
{
    if (eeprom_exists) {
        u32 temp = e1000_eeprom_read(0);
        mac[0]   = temp & 0xff;
        mac[1]   = temp >> 8;
        temp     = e1000_eeprom_read(1);
        mac[2]   = temp & 0xff;
        mac[3]   = temp >> 8;
        temp     = e1000_eeprom_read(2);
        mac[4]   = temp & 0xff;
        mac[5]   = temp >> 8;
    } else {
        const u8 *mem_base_mac_8   = (u8 *)(uptr)(mem_base + 0x5400);
        const u32 *mem_base_mac_32 = (u32 *)(uptr)(mem_base + 0x5400);
        if (mem_base_mac_32[0] != 0) {
            for (int i = 0; i < 6; i++) {
                mac[i] = mem_base_mac_8[i];
            }
        } else {
            return false;
        }
    }

    // network_set_mac(mac);
    return true;
}

/**
 * @brief Initialise receive descriptors and configure the controller for RX.
 */
void e1000_rx_init()
{
    // const u8 *ptr = (u8 *)(kmalloc(sizeof(struct e1000_rx_desc) * E1000_RX_RING_SIZE + 16));
    //
    // const struct e1000_rx_desc *descs = (struct e1000_rx_desc *)ptr;
    //
    // for (int i = 0; i < E1000_RX_RING_SIZE; i++) {
    //     rx_descs[i]         = (struct e1000_rx_desc *)((u8 *)descs + i * 16);
    //     rx_descs[i]->addr   = (u64)(uptr)(kmalloc(8192 + 16));
    //     rx_descs[i]->status = 0;
    // }
    //
    // e1000_write_command(REG_RXDESCLO, (u32)ptr);
    // e1000_write_command(REG_RXDESCHI, 0);
    //
    // e1000_write_command(REG_RXDESCLEN, E1000_RX_RING_SIZE * 16);
    //
    // e1000_write_command(REG_RXDESCHEAD, 0);
    // e1000_write_command(REG_RXDESCTAIL, E1000_RX_RING_SIZE - 1);
    // rx_cur = 0;
    // e1000_write_command(REG_RCTRL,
    //                     RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC |
    //                     RCTL_BSIZE_8192);
}

/**
 * @brief Initialise transmit descriptors and enable transmission.
 */
void e1000_tx_init()
{
    // const u8 *ptr = (u8 *)(kmalloc(sizeof(struct e1000_tx_desc) * E1000_TX_RING_SIZE + 16));
    //
    // const struct e1000_tx_desc *descs = (struct e1000_tx_desc *)ptr;
    //
    // for (int i = 0; i < E1000_TX_RING_SIZE; i++) {
    //     tx_descs[i]         = (struct e1000_tx_desc *)((u8 *)descs + i * 16);
    //     tx_descs[i]->addr   = 0;
    //     tx_descs[i]->cmd    = 0;
    //     tx_descs[i]->status = TSTA_DD;
    // }
    //
    // e1000_write_command(REG_TXDESCLO, (u32)ptr);
    // e1000_write_command(REG_TXDESCHI, 0);
    //
    // e1000_write_command(REG_TXDESCLEN, E1000_TX_RING_SIZE * 16);
    //
    // e1000_write_command(REG_TXDESCHEAD, 0);
    // e1000_write_command(REG_TXDESCTAIL, 0);
    // tx_cur = 0;
    // e1000_write_command(REG_TCTRL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT) | TCTL_RTLC);

    // This line of code overrides the one before it but I left both to highlight that the previous one works with e1000
    // cards, but for the e1000e cards you should set the TCTRL register as follows. For detailed description of each
    // bit, please refer to the Intel Manual. In the case of I217 and 82577LM packets will not be sent if the TCTRL is
    // not configured using the following bits.
    // e1000_write_command(REG_TCTRL, 0b0110000000000111111000011111010);
    // e1000_write_command(REG_TIPG, 0x0060200A);
}

/**
 * @brief Unmask e1000 interrupts and clear pending status bits.
 */
void e1000_enable_interrupt()
{
    e1000_write_command(REG_IMS, E1000_IMS_ENABLE_MASK);
    e1000_read_command(REG_ICR);
}

/**
 * @brief Probe PCI resources and start the e1000 network controller.
 *
 * @param pci PCI device descriptor for the controller.
 */
void e1000_init(struct pci_device pci)
{
    pci_device = pci;
    bar_type   = pci_get_bar(pci_device, PCI_BAR_MEM) & 1;
    io_base    = pci_get_bar(pci_device, PCI_BAR_IO) & ~1;
    mem_base   = pci_get_bar(pci_device, PCI_BAR_MEM) & ~3;

    pci_enable_bus_mastering(pci);
    eeprom_exists = false;
    if (e1000_start()) {
        // arp_init();
    } else {
        boot_message(WARNING_LEVEL_ERROR, "E1000 failed to start");
    }
}

/**
 * @brief Interrupt service routine for e1000 events.
 *
 * @param frame Interrupt context frame.
 */
void e1000_interrupt_handler(struct trapframe *frame)
{
    int interrupt = frame->trapno;
    if (interrupt == pci_device.header.irq + IRQ0) {
        // This might be needed here if your handler doesn't clear interrupts from each device and must be done
        // before EOI if using the PIC. Without this, the card will spam interrupts as the int-line will stay high.
        e1000_write_command(REG_IMS, 0x1);

        // - Bit 0 (0x01): Transmit Descriptor Written Back (TXDW) - Indicates that the transmit descriptor has been
        // written back.
        // - Bit 1 (0x02): Transmit Queue Empty (TXQE) - Indicates that the transmit queue is empty.
        // - Bit 2 (0x04): Link Status Change (LSC) - Indicates a change in the link status.
        // - Bit 3 (0x08): Receive Descriptor Minimum Threshold (RXDMT0) - Indicates that the receive descriptor
        // minimum threshold has been reached.
        // - Bit 4 (0x10): Good Threshold (GPI) - Indicates a good threshold event.
        // - Bit 6 (0x40): Receive Timer Interrupt (RXT0) - Indicates that the receive timer has expired.
        // - Bit 7 (0x80): Receive Descriptor Written Back (RXO) - Indicates that a receive descriptor has been
        // written back.
        const u32 status = e1000_read_command(REG_ICR);
        if (status & E1000_LSC) {
            e1000_linkup();
        } else if (status & E1000_RXDMT0) {
            // triggered when the number of packets in the receive buffer is above the minimum threshold
            e1000_receive();
        } else if (status & E1000_RX0) {
            // triggered as soon as any packet is received
            e1000_receive();
        } else if (status & E1000_RXT0) {
            // triggered when a packet has been sitting in the receive buffer for a certain amount of time
            e1000_receive();
        }

        e1000_write_command(REG_IMS, E1000_IMS_ENABLE_MASK);
    }
}

/**
 * @brief Log the detected MAC address to the console.
 */
void e1000_print_mac_address()
{
    // boot_message(WARNING_LEVEL_INFO, "Intel e1000 MAC Address: %s", get_mac_address_string(mac));
}

/**
 * @brief Force the link-up state in the controller's control register.
 */
void e1000_linkup()
{
    u32 val = e1000_read_command(REG_CTRL);
    val |= ECTRL_SLU;
    e1000_write_command(REG_CTRL, val);
}

/**
 * @brief Bring the controller online, register interrupts, and start queues.
 *
 * @return true on success, false otherwise.
 */
bool e1000_start()
{
    return false;
    e1000_detect_eeprom();
    if (!e1000_read_mac_address()) {
        return false;
    }
    e1000_print_mac_address();
    e1000_linkup();

    // Clear multicast table array
    for (int i = 0; i < 0x80; i++) {
        e1000_write_command(REG_MTA + i * 4, 0);
    }

    // if (idt_register_interrupt_callback(IRQ0 + pci_device.header.irq, e1000_interrupt_handler) == 0) {
    //     e1000_enable_interrupt();
    //     e1000_rx_init();
    //     e1000_tx_init();
    //
    //     // dhcp_send_discover(mac);
    //     return true;
    // }
    return false;
}

/// @brief Process all available packets in the receive ring
/**
 * @brief Process all packets currently available in the receive ring.
 */
void e1000_receive()
{
    while ((rx_descs[rx_cur]->status & E1000_RXD_STAT_DD)) {
        auto const buf = (u8 *)(u32)rx_descs[rx_cur]->addr;
        const u16 len  = rx_descs[rx_cur]->length;

        if (!(rx_descs[rx_cur]->status & E1000_RXD_STAT_EOP)) {
            // TODO: Handle the error: packet is not complete
            // warningf("Incomplete packet received\n");
            continue;
        }

        // network_receive(buf, len);

        rx_descs[rx_cur]->status = 0;
        const u16 old_cur        = rx_cur;
        rx_cur                   = (rx_cur + 1) % E1000_RX_RING_SIZE;
        e1000_write_command(REG_RXDESCTAIL, old_cur);
    }
}

/**
 * @brief Submit a frame for transmission.
 *
 * @param data Pointer to the Ethernet frame.
 * @param len Frame length in bytes.
 * @return 0 on success.
 */
int e1000_send_packet(const void *data, const u16 len)
{
    tx_descs[tx_cur]->addr   = (uptr)data;
    tx_descs[tx_cur]->length = len;
    tx_descs[tx_cur]->cmd    = CMD_EOP | CMD_IFCS | CMD_RS;
    tx_descs[tx_cur]->status = 0;
    const u8 old_cur         = tx_cur;
    tx_cur                   = (tx_cur + 1) % E1000_TX_RING_SIZE;
    e1000_write_command(REG_TXDESCTAIL, tx_cur);
    while (!(tx_descs[old_cur]->status & 0xff));

    return 0;
}