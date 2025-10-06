#pragma once
#include <stdint.h>
#include <proc.h>

void seginit(void);
void kvmalloc(void);
uintptr_t* setupkvm(void);
char* uva2ka(uintptr_t*, char*);
int allocuvm(uintptr_t*, uint32_t, uint32_t);
int deallocuvm(uintptr_t*, uint32_t, uint32_t);
void freevm(uintptr_t*);
void inituvm(uintptr_t*, const char*, uint32_t);
int loaduvm(uintptr_t*, char*, struct inode*, uint32_t, uint32_t);
uintptr_t* copyuvm(uintptr_t*, uint32_t);
void switch_uvm(struct proc*);
void switch_kvm(void);
int copyout(uintptr_t*, uint32_t, void*, uint32_t);
void clearpteu(uintptr_t* pgdir, const char* uva);
