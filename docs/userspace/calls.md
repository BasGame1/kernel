# Userspace System Calls (kcalls)

## Overview
The kernel implements a minimalist microkernel system call interface (`kcalls`). All kernel services, drivers, and user tasks communicate across privilege boundaries via standard architectural traps. The core kernel exposes only fundamental primitives: context yielding, synchronous inter-process communication (IPC), service discovery, and privileged hardware mapping.

---

## Calling Convention & ABI

### Register Layout across Architectures

#### RISC-V (RV32 / RV64)
- Trap instruction: `ecall`
- Call vector: `trap_handler` via `stvec` / `mtvec`
- System Call ID: Register `a7`
- Arguments: `a0`, `a1`, `a2`, `a3`, `a4`, `a5`
- Return value: Register `a0`
- State preservation: All 32 general-purpose registers saved in `struct trap_frame` on kernel stack.

#### x86 (IA-32 / x86_64)
- Trap instruction: `int 0x80` / `syscall`
- Call ID: Register `eax` / `rax`
- Arguments: `ebx`, `ecx`, `edx`, `esi`, `edi`, `ebp` (IA-32) or `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9` (x86_64)
- Return value: `eax` / `rax`

#### ARM (AArch32 / AArch64)
- Trap instruction: `svc 0`
- Call ID: Register `r7` (AArch32) / `x8` (AArch64)
- Arguments: `r0`-`r5` (AArch32) / `x0`-`x5` (AArch64)
- Return value: `r0` / `x0`

---

## System Call Table

| ID | Constant | Function Prototype | Privilege | Description |
|---|---|---|---|---|
| `0` | `KCALL_YIELD` | `uint32_t user_yield(void)` | Unprivileged | Voluntarily yields current time slice to scheduler |
| `1` | `KCALL_IPC_SEND` | `uint32_t user_ipc_send(uint32_t dest_pid, void *msg)` | Capability checked | Synchronous message transmission to destination process |
| `2` | `KCALL_IPC_RECV` | `uint32_t user_ipc_recv(uint32_t src_pid, void *buf)` | Capability checked | Synchronous message reception from source process |
| `3` | `KCALL_IPC_CALL` | `uint32_t user_ipc_call(uint32_t dest_pid, void *msg, void *reply)` | Capability checked | Atomic bidirectional synchronous RPC transaction |
| `4` | `KCALL_GET_PID_BY_GID` | `uint32_t user_get_pid_by_gid(uint32_t gid)` | Unprivileged | Resolves process ID for designated capability Group ID |
| `10` | `PKCALL_MAP_DEVICE` | `uint32_t pkcall_map_device(uintptr_t phys, uintptr_t virt)` | Privileged (`SID`/`GID`) | Maps physical device MMIO range into address space |

---

## Detailed Call Specifications

### 0: `KCALL_YIELD`
```c
uint32_t user_yield(void);
```
- **Description**: Relinquishes remaining CPU quantum. Places calling process at tail of round-robin runnable queue and dispatches next `PROCESS_READY` thread.
- **Parameters**: None.
- **Return Value**: `0` on successful dispatch.
- **Side Effects**: Modifies process state from `PROCESS_RUNNING` to `PROCESS_READY`; triggers immediate context switch.

### 1: `KCALL_IPC_SEND`
```c
uint32_t user_ipc_send(uint32_t dest_pid, void *message_ptr);
```
- **Description**: Initiates synchronous message transfer to `dest_pid`.
- **Validation**:
  - `dest_pid` must exist and be within range `[MIN_PID, MAX_PID]`.
  - `message_ptr` validated via `is_valid_user_buf(message_ptr, 64)`.
  - Sender and receiver GID relationship validated via `check_gid_permission()`.
- **Rendezvous Logic**:
  - If target process is in state `PROCESS_BLOCKED_RECV` waiting for sender:
    - Kernel copies 64 bytes directly from `message_ptr` to target `ipc_msg_buf`.
    - Target process state set to `PROCESS_READY`.
    - Caller continues running or yields depending on scheduling policy.
  - If target process is not awaiting message:
    - Calling process state set to `PROCESS_BLOCKED_SEND`.
    - Sender `ipc_target_pid` set to `dest_pid`; `ipc_msg_buf` recorded.
    - Scheduler dispatches next runnable thread.
- **Return Value**: `0` on success; negative error code on failure.

### 2: `KCALL_IPC_RECV`
```c
uint32_t user_ipc_recv(uint32_t src_pid, void *message_buf);
```
- **Description**: Receives 64-byte message from `src_pid`. If `src_pid == 0`, receives from any authorized sender.
- **Validation**:
  - `message_buf` validated via `is_valid_user_buf(message_buf, 64)`.
- **Rendezvous Logic**:
  - If matching sender is blocked in `PROCESS_BLOCKED_SEND`:
    - Kernel copies 64 bytes from sender buffer to `message_buf`.
    - Blocked sender transitioned to `PROCESS_READY`.
    - Receiver remains `PROCESS_RUNNING`.
  - If no sender is pending:
    - Receiver transitioned to `PROCESS_BLOCKED_RECV`.
    - Receiver `ipc_target_pid` set to `src_pid`; `ipc_msg_buf` recorded.
    - Scheduler dispatches next thread.
- **Return Value**: `0` on success; negative error code on failure.

### 3: `KCALL_IPC_CALL`
```c
uint32_t user_ipc_call(uint32_t dest_pid, void *msg, void *reply);
```
- **Description**: Atomic RPC transaction combining `user_ipc_send` and `user_ipc_recv`. Eliminates intermediate context switch overhead by immediately blocking caller on reply from `dest_pid`.
- **Parameters**:
  - `dest_pid`: Target service PID.
  - `msg`: Pointer to 64-byte outgoing request.
  - `reply`: Pointer to 64-byte buffer for incoming response.
- **Return Value**: `0` on transaction completion; negative on transmission error or timeout.

### 4: `KCALL_GET_PID_BY_GID`
```c
uint32_t user_get_pid_by_gid(uint32_t gid);
```
- **Description**: Queries kernel process table to resolve system service managing specified Group ID (GID).
- **Parameters**:
  - `gid`: Capability group identifier (e.g., `FS_GID`, `SCREEN_GID`, `ANTARTICA_GID`).
- **Return Value**: Process ID (`pid >= MIN_PID`) of registered service, or `0` if service not running.

### 10: `PKCALL_MAP_DEVICE`

> [!CAUTION]
> Private kcall, restricted to services with SID, [explained here](../security/security.md)

```c
uint32_t pkcall_map_device(uintptr_t phys_addr, uintptr_t virt_addr);
```
- **Description**: Maps physical MMIO register page into caller address space.
- **Security Check**:
  - Verified via `verify_pkcall(current_process->gid, current_process->sid)`.
  - Process must possess hardware capability GID (`HW_GID`, `SCREEN_GID`, `KEYBOARD_GID`, `VIRTIO_GID`, `GPU_GID`).
  - Cryptographic Security Identifier (`sid`) verified against entry point signature generated at process spawn.
- **Page Table Modifications**:
  - Creates PTE with `PTE_V | PTE_R | PTE_W | PTE_U` (W^X strictly enforced: `PTE_X` never set).
- **Return Value**: `0` on success; `1` on authorization failure; negative on page table allocation error.

---

## Memory Safety & Pointer Sanitization
All userspace buffer pointers passed to kcalls undergo mandatory hardware and software sanity checks:
1. **Null Pointer Check**: Buffer address must not be zero.
2. **Userspace Range Verification**:
   ```c
   bool is_valid_user_buf(const void *buf, size_t len) {
      uintptr_t start = (uintptr_t)buf;
      uintptr_t end = start + len;
      if (end < start) return false; // Overflow check
      return (start >= USER_MEM_START && end <= USER_MEM_END);
   }
   ```
3. **Alignment**: 64-byte IPC messages must adhere to word alignment boundaries (`sizeof(uintptr_t)`).
