#pragma once
#include "spinlock.h"
#include "proc.h"

struct process_queue
{
    struct proc *head;
    struct proc *tail;
    struct spinlock lock;
    int size;
};

void switch_context(struct context **, struct context *);
void scheduler(void) __attribute__ ((noreturn));
void switch_to_scheduler(void);
void procdump(void);
struct proc *dequeue_task(struct process_queue *queue);
void enqueue_task(struct process_queue *queue, struct proc *task);

void enqueue_runnable(struct proc *process);
struct proc *dequeue_runnable();
void enqueue_sleeping(struct proc *process);
struct proc *dequeue_sleeping();
void enqueue_zombie(struct proc *process);
struct proc *dequeue_zombie();
