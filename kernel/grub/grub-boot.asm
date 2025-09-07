; Declare constants for the multiboot header.
MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
VIDEOMODE equ  1 << 2            ; request video mode
MBFLAGS  equ  MBALIGN | MEMINFO | VIDEOMODE ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + MBFLAGS)   ; checksum of above, to prove we are multiboot

section .multiboot
align 4
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

section .bss
align 16
stack_bottom:
resb 1048576 ; 1 MiB
global kernel_stack_top
kernel_stack_top:

section .text
global _start:function (_start.end - _start)
_start:
    ; Set up the stack
	mov esp, kernel_stack_top

    ; Push the multiboot header and the magic number as parameter to be used in kernel_main
    push eax
    push ebx

	extern kernel_main
	call kernel_main

	cli
.hang:	hlt
	jmp .hang
.end: