#include <process.h>
#include <stdint.h>
#include <syscall.h>
#include <task.h>
#include <timer.h>

void *sys_sleep(void)
{
    uint64_t start_time         = get_cpu_time_ns();
    const uint32_t milliseconds = get_integer_argument(0);
    // 1 millisecond = 1'000'000 nanoseconds, but it seems get_cpu_time_ns() returns in tens of nanoseconds
    // so I took one zero out.
    uint64_t nanoseconds = milliseconds * 1'000'00;

    acquire(&tickslock);
    while (get_cpu_time_ns() - start_time < nanoseconds) {
        if (current_process()->killed) {
            release(&tickslock);
            return (void *)-1;
        }
        sleep((void *)&timer_tick, &tickslock);
    }
    release(&tickslock);

    return nullptr;
}
