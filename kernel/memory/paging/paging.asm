[BITS 32]

section .text

global enable_paging

extern print

enable_paging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, (1 << 31)  ; Set the paging bit in CR0
    mov cr0, eax
    pop ebp
    
    ret