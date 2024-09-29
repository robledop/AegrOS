[BITS 32]

section .asm

global paging_load_directory
global enable_paging

extern print_line

paging_load_directory:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov cr3, eax
    pop ebp   
    ; push message1
    ; call print_line
    ; add esp, 4
    ret

enable_paging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    pop ebp
    ; push message2
    ; call print_line
    ; add esp, 4
    ret

 
; section .data
; message1 db "Paging directory loaded", 0
; message2 db "Paging enabled", 0