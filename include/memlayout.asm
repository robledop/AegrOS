; AUTO-GENERATED from memlayout.h
; DO NOT EDIT - Changes will be overwritten on recompile

%ifndef _memlayout
%define _memlayout
%define EXTMEM 0x100000    ; Start of extended memory (1MB)
%define PHYSTOP 0x20000000 ; Top physical memory (512MB)
%define DEVSPACE 0xFD000000
%define KERNBASE 0x80000000          ; First kernel virtual address (2GB)
%define KERNLINK (KERNBASE + EXTMEM) ; Address where kernel is linked (2GB + 1MB))
%define V2P(a) ((unsigned int)(a) - KERNBASE)
%define P2V(a) ((void *)((unsigned int)(a) + KERNBASE))
%define V2P_WO(x) ((x) - KERNBASE) ; same as V2P, but without casts
%define P2V_WO(x) ((x) + KERNBASE) ; same as P2V, but without casts

%endif
