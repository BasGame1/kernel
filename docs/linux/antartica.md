# Antartica Linux Syscall Compatibility Layer

## Overview
**Antartica** is a userspace Linux ABI emulation subsystem. It enables standard, unmodified Linux binaries (ELF executables) to execute on top of the microkernel by intercepting Linux system calls and translating them into native microkernel IPC requests. 
To activate use `USE_ANTARTICA` in config
Antartica runs entirely in user space as an isolated service assigned `ANTARTICA_GID` (12).

---

## Jump Table
Antartica maintains a jump table indexed by Linux system call number:
```c
extern const linux_syscall_fn linux_syscall_table[LINUX_SYSCALL_COUNT];
```

Unimplemented syscalls resolve to `sys_ni_syscall`, which gracefully returns `-LINUX_ENOSYS` (`-38`).

### Core Implemented System Calls

These are some of the most core syscalls from linux applied to conversion

#### File & I/O Subsystem
- `sys_openat` (`56`): Translates relative and absolute paths into VFS lookup requests. Allocates file descriptor in process table.
- `sys_read` (`63`) / `sys_write` (`64`): Performs byte transfers. Standard I/O (FDs 0, 1, 2) routed to keyboard and screen/console services; file FDs routed via IPC to `FS_GID`.
- `sys_close` (`57`): Flushes buffers and releases file descriptor slot.
- `sys_lseek` (`62`): Adjusts file offset pointer.
- `sys_newfstatat` (`79`) / `sys_fstat` (`80`): Fills `struct linux_stat` with file metadata, size, permissions, and timestamps.
- `sys_mkdirat` (`34`) / `sys_unlinkat` (`35`): Directory creation and file unlinking.
- `sys_getdents64` (`61`): Reads directory entries formatted as `struct linux_dirent64`.

#### 2. Memory & Heap Management
- `sys_brk` (`214`): Emulates heap adjustment pointer. Allocates or frees pages via `allocate_page()` and updates user address space.
- `sys_mmap` (`222`): Allocates anonymous memory or maps file backing pages into address space. Enforces W^X page protection.
- `sys_munmap` (`215`): Unmaps specified virtual address range and frees physical frames.
- `sys_mprotect` (`226`): Updates page protection flags (`PROT_READ`, `PROT_WRITE`, `PROT_EXEC`) while enforcing W^X compliance.

#### Process & Thread Management
- `sys_clone` (`220`): Spawns child process or thread with designated user stack.
- `sys_exit` (`93`) / `sys_exit_group` (`94`): Terminates process and frees allocated resource tables.
- `sys_wait4` (`244`): Awaits status change in child processes.
- `sys_getpid` (`172`), `sys_getppid` (`173`), `sys_getuid` (`174`), `sys_getgid` (`176`): Returns process identity attributes.
- `sys_sched_yield` (`124`): Delegates to native `user_yield()`.

#### Time, Clocks & Randomness
- `sys_clock_gettime` (`113`): Formats hardware timer ticks into `struct linux_timespec` (seconds and nanoseconds).
- `sys_gettimeofday` (`169`): Returns current time in `struct linux_timeval`.
- `sys_nanosleep` (`101`): Suspends thread execution for specified duration using timer ticks.
- `sys_getrandom` (`262`): Returns cryptographic random bytes sourced from Neo enclave or hardware TRNG.

## Linux Error Code Mapping
Antartica returns negative error codes conforming exactly to Linux ABI conventions:
- `-LINUX_EPERM` (`-1`): Operation not permitted.
- `-LINUX_ENOENT` (`-2`): No such file or directory.
- `-LINUX_EBADF` (`-9`): Bad file descriptor.
- `-LINUX_ENOMEM` (`-12`): Out of memory.
- `-LINUX_EACCES` (`-13`): Permission denied.
- `-LINUX_EEXIST` (`-17`): File exists.
- `-LINUX_EINVAL` (`-22`): Invalid argument.
- `-LINUX_ENOSYS` (`-38`): Function not implemented.
