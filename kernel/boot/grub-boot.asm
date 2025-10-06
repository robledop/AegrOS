%include "memlayout.asm"
; Declare constants for the multiboot header.
MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
VIDEOMODE equ  1 << 2            ; request video mode
%ifdef PIXEL_RENDERING
MBFLAGS equ MBALIGN | MEMINFO | VIDEOMODE
%else
MBFLAGS equ MBALIGN | MEMINFO
%endif
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + MBFLAGS)   ; checksum of above, to prove we are multiboot

align 4
section .text
global multiboot_header
multiboot_header:
	dd MAGIC
	dd MBFLAGS
	dd CHECKSUM
    ; Video mode fields (required when VIDEOMODE flag is set)
	dd 0    ; header_addr (not used)
	dd 0    ; load_addr (not used)
	dd 0    ; load_end_addr (not used)
	dd 0    ; bss_end_addr (not used)
	dd 0    ; entry_addr (not used)
	dd 0    ; mode_type (0 = linear graphics mode)
	dd 1024 ; width
	dd 768  ; height
	dd 32   ; depth

;global _start:function (_start.end - _start)
global _start
;_start equ V2P_WO(entry)
_start equ entry

global entry
entry:
    mov esi, eax ; Save the magic number from grub
    mov edi, ebx ; Save the address of the multiboot info structure from grub

    ; Turn on page size extension for 4MB pages
    mov eax, cr4
    or eax, 0x00000010 ; Set PSE bit
    mov cr4, eax

    ; Set page directory
    extern entrypgdir
    mov eax, V2P_WO(entrypgdir)
    mov cr3, eax
    ; Enable paging
    mov eax, cr0
    or eax, 0x80010000 ; Set PG bit and write-protect bit
    mov cr0, eax

    ; Set up the stack
	mov esp, kernel_stack_top
;
    ; Manually set up the call frame for kernel_main
    push esi        ; Second parameter (magic number)
    push edi        ; First parameter (multiboot info)
    push 0          ; Fake return address (kernel_main shouldn't return anyway)
;
    ; Jump to kernel_main instead of calling it because we need it to run at a higher address
	extern kernel_main
	mov eax, kernel_main
    jmp eax
;	call kernel_main
;
	cli
.hang:	hlt
	jmp .hang
.end:

section .bss
align 16
stack_bottom:
resb 4096 ; 4KB stack
global kernel_stack_top
kernel_stack_top:
