[BITS 32]

section .asm

global _start
extern c_start

_start:
    pop eax        ; Remove the fake return address setup by exec from stack
    call c_start
    ret