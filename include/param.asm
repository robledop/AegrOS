; AUTO-GENERATED from param.h
; DO NOT EDIT - Changes will be overwritten on recompile

%ifndef _param
%define _param
%define NPROC        64  ; maximum number of processes
%define KSTACKSIZE 4096  ; size of per-process kernel stack
%define NCPU          8  ; maximum number of CPUs
%define NOFILE       16  ; open files per process
%define NFILE       100  ; open files per system
%define NINODE       50  ; maximum number of active i-nodes
%define NDEV         10  ; maximum major device number
%define ROOTDEV       0  ; device number of file system root disk
%define EXT2DEV       2  ; device number of file system ext2 disk
%define MAXARG       32  ; max exec arguments
%define MAXOPBLOCKS  10  ; max # of blocks any FS op writes
%define LOGSIZE      (MAXOPBLOCKS*3)  ; max data blocks in on-disk log
%define NBUF         (MAXOPBLOCKS*3)  ; size of disk block cache
%define MAX_FILE_PATH 255  ; maximum file path length
%define TIMER_FREQUENCY_HZ 50
%define TIMER_INTERVAL_MS (1000 / TIMER_FREQUENCY_HZ)
%define TIME_SLICE_MS 50
%define TIME_SLICE_TICKS ((TIME_SLICE_MS + TIMER_INTERVAL_MS - 1) / TIMER_INTERVAL_MS)

%endif
