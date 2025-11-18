#include "memlayout.h"
#include "string.h"
#include "termcolors.h"
#include "types.h"
#include "defs.h"
#include "elf.h"
#include "debug.h"
#include "printf.h"
#include "proc.h"
#include "x86.h"

typedef struct stack_frame
{
    struct stack_frame *ebp;
    u32 eip;
} stack_frame_t;

extern char kernel_end[];

static struct elf32_shdr *symtab_section_header = nullptr;
static struct elf32_shdr *strtab_section_header = nullptr;
static struct elf32_shdr *elf_section_headers   = nullptr;
// Points to the end of the reserved symbol area so we don't overwrite it
static char *reserved_symbol_end = nullptr;


NO_SSE

void stack_trace(void)
{
    printf(KBWHT "Stack trace:\n" KRESET);

    const stack_frame_t *stack = __builtin_frame_address(0);
    int max                    = 10;
    while (stack && stack->eip != 0 && max-- > 0) {
        auto const symbol = debug_function_symbol_lookup(stack->eip);
        printf("\t" KBCYN "0x%x" KBLU " [" KBWHT "%s" KWHT " + 0x%x" KBLU "]\n" KWHT,
               stack->eip,
               (symbol.name == nullptr) ? "[unknown]" : symbol.name,
               stack->eip - symbol.address);
        stack = stack->ebp;
    }

    printf(KWHT "run " KBBLU "addr2line -e build/kernel <address>" KWHT " to get line numbers\n");
    printf("run " KBBLU "objdump -d build/kernel | grep <address> -A 40 -B 40" KWHT " to see more.\n");
}

NO_SSE

void print_registers()
{
    struct proc *current_proc = current_process();
    if (!current_proc || !current_proc->trap_frame) {
        printf(KBWHT "No current process or trap frame available.\n" KRESET);
        return;
    }
    struct trapframe *frame = current_proc->trap_frame;
    printf(KBWHT "Registers for the process '%s' on cpu %d:\n" KWHT, current_proc->name, cpu_index());
    printf("\tEAX: %#10x", frame->eax);
    printf("\tEBX: %#10x", frame->ebx);
    printf("\tECX: %#10x", frame->ecx);
    printf("\tEDX: %#10x\n", frame->edx);
    printf("\tESI: %#10x", frame->esi);
    printf("\tEDI: %#10x", frame->edi);
    printf("\tEBP: %#10x", frame->ebp);
    printf("\tESP: %#10x\n", frame->esp);
    printf("\tEIP: %#10x", frame->eip);
    printf("\tCS:  %#10x", frame->cs);
    printf("\tDS:  %#10x", frame->ds);
    printf("\tSS:  %#10x\n", frame->ss);
    auto eflags = read_eflags();

    printf("\tEFLAGS: %#10x ", eflags);

    if (eflags & EFLAGS_ALL) {
        printf("( ");
    }
    if (eflags & EFLAGS_CF) {
        printf("CF ");
    }
    if (eflags & EFLAGS_PF) {
        printf("PF ");
    }
    if (eflags & EFLAGS_AF) {
        printf("AF ");
    }
    if (eflags & EFLAGS_ZF) {
        printf("ZF ");
    }
    if (eflags & EFLAGS_SF) {
        printf("SF ");
    }
    if (eflags & EFLAGS_TF) {
        printf("TF ");
    }
    if (eflags & EFLAGS_IF) {
        printf("IF ");
    }
    if (eflags & EFLAGS_DF) {
        printf("DF ");
    }
    if (eflags & EFLAGS_OF) {
        printf("OF ");
    }
    if (eflags & EFLAGS_IOPL) {
        printf("IOPL ");
    }
    if (eflags & EFLAGS_NT) {
        printf("NT ");
    }
    if (eflags & EFLAGS_RF) {
        printf("RF ");
    }
    if (eflags & EFLAGS_VM) {
        printf("VM ");
    }
    if (eflags & EFLAGS_AC) {
        printf("AC ");
    }
    if (eflags & EFLAGS_VIF) {
        printf("VIF ");
    }
    if (eflags & EFLAGS_VIP) {
        printf("VIP ");
    }
    if (eflags & EFLAGS_ID) {
        printf("ID ");
    }
    if (eflags & EFLAGS_AI) {
        printf("AI ");
    }

    if (eflags & EFLAGS_ALL) {
        printf(")");
    }
    printf("\n");
}

char *debug_reserved_end(void)
{
    if (reserved_symbol_end) {
        return reserved_symbol_end;
    }
    return kernel_end;
}

void debug_stats(void)
{
    print_registers();
    stack_trace();
}

NO_SSE

struct symbol debug_function_symbol_lookup(const elf32_addr address)
{
    if (!symtab_section_header || !strtab_section_header) {
        return (struct symbol){0, nullptr};
    }

    auto const symbols_table = (struct elf32_sym *)P2V(symtab_section_header->sh_addr);
    if (!symbols_table) {
        return (struct symbol){0, nullptr};
    }
    const u32 symtab_entry_count = symtab_section_header->sh_size / sizeof(struct elf32_sym);

    auto const strtab_addr = (const char *)P2V(strtab_section_header->sh_addr);

    const struct elf32_sym *closest_func = nullptr;
    for (u32 i = 0; i < symtab_entry_count; i++) {
        const struct elf32_sym *sym = &symbols_table[i];

        // Ensure we only return function symbols
        // if (ELF32_ST_TYPE(sym->st_info) != STT_FUNC) {
        //     continue;
        // }

        // Skip symbols with no name
        if (sym->st_name == 0) {
            continue;
        }

        // Skip undefined symbols
        if (sym->st_shndx == SHN_UNDEF) {
            continue;
        }

        // Exact match
        if (sym->st_value == address) {
            const char *symbol_name    = strtab_addr + sym->st_name;
            const struct symbol symbol = {
                .address = sym->st_value,
                .name = symbol_name,
            };
            return symbol;
        }

        // Closest match below the address so far
        if (sym->st_value < address && (!closest_func || sym->st_value > closest_func->st_value)) {
            closest_func = sym;
        }
    }

    // Return the closest match below the address if no exact match was found
    if (closest_func) {
        const char *symbol_name    = strtab_addr + closest_func->st_name;
        const struct symbol symbol = {
            .address = closest_func->st_value,
            .name = symbol_name,
        };
        return symbol;
    }

    return (struct symbol){0, nullptr};
}

NO_SSE void init_symbols(const multiboot_info_t *mbd)
{
    reserved_symbol_end = kernel_end;

    if (!(mbd->flags & MULTIBOOT_INFO_ELF_SHDR)) {
        return;
    }

    const u32 num       = mbd->u.elf_sec.num;
    elf_section_headers =
        (struct elf32_shdr *)P2V(mbd->u.elf_sec.addr);
    const auto sh_strtab = (const char *)P2V(elf_section_headers[mbd->u.elf_sec.shndx].sh_addr);

    for (u32 i = 0; i < num; i++) {
        struct elf32_shdr *sh    = &elf_section_headers[i];
        const char *section_name = sh_strtab + sh->sh_name;

        if (starts_with(".symtab", section_name)) {
            symtab_section_header = sh;
            strtab_section_header = &elf_section_headers[sh->sh_link];

            char *symtab_end = (char *)P2V(symtab_section_header->sh_addr) + symtab_section_header->sh_size;
            if (symtab_end > reserved_symbol_end) {
                reserved_symbol_end = symtab_end;
            }

            char *strtab_end = (char *)P2V(strtab_section_header->sh_addr) + strtab_section_header->sh_size;
            if (strtab_end > reserved_symbol_end) {
                reserved_symbol_end = strtab_end;
            }
            break;
        }
    }
}