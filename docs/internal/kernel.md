// docs/internal/kernel.md
# Kernel Core Architecture & Internals

## Overview
Based on a microkernel arch, almos

---

## Boot Sequence
The boot control goes to `kernel_init()`, that calls the basic services like `start_screen()` or `init_memory()` like any kernel before creating the userspace proccess like `create_keyboard_process()` and jump to userspace

> [!NOTE]
> Some critical userspace processes can be load as modules if prefered, using the config:
> `LOAD_<NAME>_AS_MODULE`

---

## Scheduler & Process Management

### Process Table
with a max of 16
> [!tip]
> If the max number of processes must be updates, change the macro `MAX_PROCESSES` in [`userspace.h`](../../include/userspace.h)
```c
struct process {
   uint32_t pid; // Proccess ID
   uint32_t gid; // Group ID, processed can send ipc to the same gid or gid 0 (print)
   uint32_t sid; // System ID, just for priviledged processed
   p_state_t state; // state of the proccess, 4 states, [docu here](#process-states)
   uintptr_t kernel_stack; // Supervisor stack pointer
   uintptr_t user_stack; // Unprivileged user stack pointer
   struct trap_frame *frame; // Saved hardware register state
   uint32_t ipc_target_pid; // Peer PID during IPC rendezvous
   void *ipc_msg_buf; // Address of pending IPC buffer
};
```

### Process States
- `PROCESS_UNUSED`: Module prepared but not used
- `PROCESS_READY`: Waiting to be activated
- `PROCESS_RUNNING`: Is activated
- `PROCESS_BLOCKED_SEND`: Waiting for recieve message (first sent)
- `PROCESS_BLOCKED_RECV`: Waiting to send message

---

## Clock & Hardware Timers
- **RISC-V**: Uses standard `mtime` and `mtimecmp` 64-bit memory-mapped registers. Preemption quantum is set to 10ms (100 Hz).
- **x86**: Programs Intel 8254 Programmable Interval Timer (PIT) on IRQ0 or Local APIC Timer.
- **ARM**: Configures ARM Generic Timer (`cntv_tval_el0` / `cntv_ctl_el0`).
