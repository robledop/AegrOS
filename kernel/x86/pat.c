#include <cpuid.h>
#include <pat.h>
#include <x86.h>

static uint8_t wc_index = 0xFF;

/**
 * @brief Check whether the processor supports the Page Attribute Table (PAT).
 *
 * PAT support is advertised via CPUID leaf 0x01 EDX[16]. When this bit is clear the
 * processor only honours the legacy MTRR/PWT/PCD cache attributes.
 *
 * @return true when PAT is available, false otherwise.
 */
bool pat_available(void)
{
    unsigned int eax, ebx, ecx, edx;
    if (!__get_cpuid(0x01, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    return (edx & (1U << 16)) != 0U;
}

/**
 * @brief Retrieve the PAT entry index configured for Write Combining.
 *
 * pat_init() selects a PAT slot (typically index 1 or 5) and programs it to WC. A
 * return value of 0xFF indicates PAT is unavailable or initialisation failed.
 */
uint8_t pat_wc_index(void) { return wc_index; }

/**
 * @brief Initialise PAT entries to enable write-combining mappings.
 *
 * Configures PAT entries 1 and 5 to the WC memory type so that paging code can map
 * framebuffer pages with better write throughput. Safely no-ops on hardware without PAT.
 */
void pat_init(void)
{
    if (!pat_available()) {
        wc_index = 0xFF;
        return;
    }

    uint64_t pat = rdmsr(MSR_IA32_PAT);

    // Layout: eight 8-bit entries. Default is 0,1,4,5, ... but we enforce entry 1 and 5 as WC.
    constexpr uint64_t entry_mask = 0xFFULL;

    constexpr uint64_t mask1 = entry_mask << 8;
    constexpr uint64_t mask5 = entry_mask << 40;

    uint64_t new_pat = pat;
    new_pat &= ~mask1;
    new_pat |= (uint64_t)0x01 << 8; // entry 1 -> WC

    new_pat &= ~mask5;
    new_pat |= (uint64_t)0x01 << 40; // entry 5 -> WC (same value with PAT bit set)

    if (new_pat != pat) {
        wrmsr(MSR_IA32_PAT, new_pat);
    }

    wc_index = 0x01;
}
