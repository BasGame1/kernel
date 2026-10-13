# Inter-Process Communication (IPC)

## Architecture
The operating system implements synchronous rendezvous inter-process communication. IPC is the sole mechanism through which isolated microkernel services and client applications exchange commands, notifications, and data buffers.

---

## Rendezvous Semantics
Microkernel IPC does not use unbounded kernel message queues. This eliminates memory exhaustion vulnerabilities and minimizes latency.

---

## Message Envelope Format
Standard IPC exchanges operate on fixed 64-byte payloads. This guarantees constant-time memory copying within the kernel trap handler.

```c
struct ipc_message {
   uint32_t command; // Subsystem specific opcode
   uint32_t status; // Return code or flags
   uint8_t  payload[56]; // Additional message
};
```

### Extended Transfers
For payloads exceeding 56 bytes (e.g. disk blocks or network frames), services utilize shared memory pages mapped through the VFS or memory subsystem using page loaning or grant tables.

---

## Security & Capability Verification
To prevent unauthorized processes from spoofing service messages or commanding protected hardware, every IPC transaction is checked against the GID capability matrix:

```c
bool check_gid_permission(uint32_t sender_pid, uint32_t receiver_pid);
```

### Access Control Rules
- System Services (`SYSTEM_GID = 0`): Authorized to communicate with all processes.
- Standard Userspace Clients:
   - Can send to `FS_GID` (Filesystem).
   - Can send to `SCREEN_GID` (Display output).
   - Can send to `ANTARTICA_GID` (Linux emulation).
- Restricted Services:
   - `UPDATE_PROCESS_GID` only accepts messages from authorized administrative processes.
   - `HW_GID` and `VIRTIO_GID` only accept commands from trusted driver coordinators.
- Spoofing Prevention:
   - The kernel identifies callers by hardware-enforced PID extracted from current trap context, making PID forgery mathematically impossible.
