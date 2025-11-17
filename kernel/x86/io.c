#include "x86.h"
#include <cpuid.h>
#include "types.h"
#include "printf.h"
#include "termcolors.h"

void cpu_print_info()
{
    u32 eax, ebx, ecx, edx;
    u32 largest_standard_function;
    char vendor[13];
    cpuid(0, &largest_standard_function, (u32 *)(vendor + 0), (u32 *)(vendor + 8), (u32 *)(vendor + 4));
    vendor[12] = '\0';

    if (largest_standard_function >= 0x01) {
        cpuid(0x01, &eax, &ebx, &ecx, &edx);

        printf("[" KBGRN " INFO " KRESET "] Features:");

        if (edx & CPUID_FEAT_EDX_PSE) {
            printf(" PSE");
        }
        if (edx & CPUID_FEAT_EDX_PAE) {
            printf(" PAE");
        }
        if (edx & CPUID_FEAT_EDX_APIC) {
            printf(" APIC");
        }
        if (edx & CPUID_FEAT_EDX_MTRR) {
            printf(" MTRR");
        }

        if (edx & CPUID_FEAT_EDX_PAT) {
            printf(" PAT");
        }

        if (edx & CPUID_FEAT_EDX_ACPI) {
            printf(" ACPI");
        }

        printf("\n");

        printf("[" KBGRN " INFO " KRESET "] Instructions:");

        if (edx & CPUID_FEAT_EDX_TSC) {
            printf(" TSC");
        }
        if (edx & CPUID_FEAT_EDX_MSR) {
            printf(" MSR");
        }
        if (edx & CPUID_FEAT_EDX_SSE) {
            printf(" SSE");
        }
        if (edx & CPUID_FEAT_EDX_SSE2) {
            printf(" SSE2");
        }
        if (ecx & CPUID_FEAT_ECX_SSE3) {
            printf(" SSE3");
        }
        if (ecx & CPUID_FEAT_ECX_SSSE3) {
            printf(" SSSE3");
        }
        if (ecx & bit_SSE4_1) {
            printf(" SSE41");
        }
        if (ecx & bit_SSE4_2) {
            printf(" SSE42");
        }
        if (ecx & bit_AVX) {
            printf(" AVX");
        }
        if (ecx & bit_F16C) {
            printf(" F16C");
        }
        if (ecx & bit_RDRND) {
            printf(" RDRAND");
        }

        printf("\n");
    }

    // Extended Function 0x00 - Largest Extended Function
    u32 largestExtendedFunc;
    cpuid(0x80000000, &largestExtendedFunc, &ebx, &ecx, &edx);

    // Extended Function 0x01 - Extended Feature Bits
    if (largestExtendedFunc >= 0x80000001) {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

        if (edx & CPUID_FEAT_EDX_IA64) {
            printf("64-bit Architecture\n");
        }
    }

    // Extended Function 0x02-0x04 - Processor Name / Brand String
    if (largestExtendedFunc >= 0x80000004) {
        char name[48];
        cpuid(0x80000002,
              (u32 *)(name + 0),
              (u32 *)(name + 4),
              (u32 *)(name + 8),
              (u32 *)(name + 12));
        cpuid(0x80000003,
              (u32 *)(name + 16),
              (u32 *)(name + 20),
              (u32 *)(name + 24),
              (u32 *)(name + 28));
        cpuid(0x80000004,
              (u32 *)(name + 32),
              (u32 *)(name + 36),
              (u32 *)(name + 40),
              (u32 *)(name + 44));

        // Processor name is right justified with leading spaces
        const char *p = name;
        while (*p == ' ') {
            ++p;
        }

        printf("[" KBGRN " INFO " KRESET "] CPU Name: %s\n", p);
    }
}