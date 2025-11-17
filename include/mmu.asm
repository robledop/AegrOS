; AUTO-GENERATED from mmu.h
; DO NOT EDIT - Changes will be overwritten on recompile

%ifndef _mmu
%define _mmu
%define FL_IF           0x00000200      ; Interrupt Enable
%define CR0_PE          0x00000001      ; Protection Enable
%define CR0_MP          0x00000002      ; Monitor Coprocessor
%define CR0_EM          0x00000004      ; Emulation
%define CR0_TS          0x00000008      ; Task Switched
%define CR0_NE          0x00000020      ; Numeric Error
%define CR0_WP          0x00010000      ; Write Protect
%define CR0_PG          0x80000000      ; Paging
%define CR4_OSFXSR      0x00000200      ; OS supports FXSAVE/FXRSTOR
%define CR4_OSXMMEXCPT  0x00000400      ; OS supports unmasked SSE exceptions
%define CR4_PSE         0x00000010      ; Page size extension
%define SEG_KCODE 1  ; kernel code
%define SEG_KDATA 2  ; kernel data+stack
%define SEG_UCODE 3  ; user code
%define SEG_UDATA 4  ; user data+stack
%define SEG_TSS   5  ; this processs task state
%define NSEGS     6
%define SEG(type, base, lim, dpl) (struct segdesc)    \
%define SEG16(type, base, lim, dpl) (struct segdesc)  \
%define DPL_USER    0x3     ; User DPL
%define STA_X       0x8     ; Executable segment
%define STA_W       0x2     ; Writeable (non-executable segments)
%define STA_R       0x2     ; Readable (executable segments)
%define STS_T32A    0x9     ; Available 32-bit TSS
%define STS_IG32    0xE     ; 32-bit Interrupt Gate
%define STS_TG32    0xF     ; 32-bit Trap Gate
%define PDX(va)         (((u32)(va) >> PDXSHIFT) & 0x3FF)
%define PTX(va)         (((u32)(va) >> PTXSHIFT) & 0x3FF)
%define PGADDR(d, t, o) ((u32)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))
%define NPDENTRIES      1024    ; # directory entries per page directory
%define NPTENTRIES      1024    ; # PTEs per page table
%define PGSIZE          4096    ; bytes mapped by a page
%define PTXSHIFT        12      ; offset of PTX in a linear address
%define PDXSHIFT        22      ; offset of PDX in a linear address
%define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1)) ; round up to the next page boundary
%define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1)) ; round down to the page boundary
%define PTE_P           0x001   ; Present
%define PTE_W           0x002   ; Writeable
%define PTE_U           0x004   ; User
%define PTE_PWT         0x008   ; Write-Through
%define PTE_PCD         0x010   ; Cache-Disable
%define PTE_PS          0x080   ; Page Size (4MB pages) / PAT bit in PTEs
%define PTE_PAT PTE_PS
%define PTE_ADDR(pte)   ((u32)(pte) & ~0xFFF)
%define PTE_FLAGS(pte)  ((u32)(pte) &  0xFFF)
%define SETGATE(gate, istrap, sel, off, d)                \

%endif
