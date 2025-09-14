# AegrOS

Just a little operating system I'm developing for fun.  
Written in C23, so it requires a version of GCC that supports it.

## Documentation

[Things I learned](docs/THINGS_I_LEARNED.md)

<details>
<summary>Screenshots</summary>

![Screenshot](docs/img/screenshot6.png)

![Screenshot](docs/img/screenshot0.png)

![Screenshot](docs/img/screenshot1.png)

![Screenshot](docs/img/screenshot2.png)

![Screenshot](docs/img/screenshot3.png)

![Screenshot](docs/img/screenshot4.png)

![Screenshot](docs/img/screenshot5.png)

</details>


<details>
<summary>Roadmap</summary>

<details>
<summary>General features</summary>

- ✅ Bootloader
- ✅ GRUB compatibility
- ✅ GDT
- ✅ TSS
- ✅ Paging
- ✅ IDT
- ✅ ATA PIO
- ✅ FAT16 - read
- ✅ FAT16 - write
- ⬜ MBR
- ✅ User mode
- ✅ Idle thread
- ✅ Spinlock
- ⬜ Semaphore
- ✅ Multi-tasking
- ⬜ User mode multi-threading
- ✅ PS/2 Keyboard
- ⬜ USB
- ✅ ELF loader
- ✅ Binary loader
- ✅ User programs
- ✅ User shell
- ✅ Serial
- ☑️ User standard library (in progress)
- ✅ Framebuffer text mode
- ✅ TTY
- ✅ PIT
- ✅ Panic with stack trace
- ⬜ DWARF debugging
- ✅ Undefined behavior sanitizer
- ✅ Stack smashing protector
- ✅ VFS
- ☑️ Network stack (in progress)
- ⬜ Make the syscalls more POSIX-like
- ☑️ GUI (in progress)

</details>

<details>
<summary>Syscalls</summary>

- ✅ fork
- ✅ exec
- ✅ create_process
- ✅ exit
- ⬜ kill
- ✅ wait
- ✅ sleep
- ✅ yield
- ✅ getpid
- ✅ open
- ✅ close
- ✅ read
- ✅ write
- ✅ lseek
- ✅ fstat
- ✅ getcwd (get_current_directory)
- ✅ chdir (set_current_directory)
- ✅ reboot
- ✅ shutdown
- ✅ malloc
- ✅ free
- ✅ calloc
- ✅ realloc
- ⬜ brk
- ⬜ sbrk
- ⬜ mmap
- ⬜ munmap
- ✅ print
- ✅ getkey
- ✅ open_dir
- ⬜ dup
- ⬜ dup2
- ⬜ pipe
- ☑️ ioctl (in progress)
- ✅ opendir
- ✅ readdir
- ✅ closedir
- ⬜ signal
- ⬜ sigaction
- ⬜ fcntl
- ⬜ socket
- ⬜ connect
- ⬜ bind
- ⬜ listen
- ⬜ accept
- ⬜ gettimeoftheday
- ⬜ clock_gettime
- ⬜ nanosleep
- ⬜ time
- ⬜ errno
- ⬜ pthread_create

</details>

</details>