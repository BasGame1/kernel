# Memory Management Subsystem

## Overview
Uses virtual memory to allocate the space for programs
---

## Virtual Memory Paging Architectures

### Supported Paging Models

#### RISC-V Sv32 (32-bit Mode)
- **Virtual Address**: 32-bit (`VPN[1]` [10b] | `VPN[0]` [10b] | `Offset` [12b])
- **Page Size**: 4 KiB base page, 4 MiB superpage.
- **Root Table**: 1024 entries of 4 bytes each (4 KiB table).
- **MMU Register**: `satp` (Mode 1 for Sv32, ASID, Root PPN).

#### RISC-V Sv39 (64-bit Mode)
- **Virtual Address**: 39-bit canonical (`VPN[2]` [9b] | `VPN[1]` [9b] | `VPN[0]` [9b] | `Offset` [12b])
- **Page Size**: 4 KiB page, 2 MiB megapage, 1 GiB gigapage.
- **Root Table**: 512 entries of 8 bytes each (4 KiB table).
- **MMU Register**: `satp` (Mode 8 for Sv39, ASID, Root PPN).

> [!NOTE]
> Paging for x86 and arm will come soon

### Page Table Entry (PTE) Flags
```c
#define PTE_V (1 << 0) // Valid
#define PTE_R (1 << 1) // Read
#define PTE_W (1 << 2) // Write
#define PTE_X (1 << 3) // Execute
#define PTE_U (1 << 4) // User-mode accessible
#define PTE_G (1 << 5) // Global mapping
#define PTE_A (1 << 6) // Accessed
#define PTE_D (1 << 7) // Dirty
```

---

## Security Enforcements

### W^X Policy (Write XOR Execute)
The page table mapping function enforces that no memory mapping may be simultaneously writable and executable:
```c
int map_page(uintptr_t *root_pt, uintptr_t virt, uintptr_t phys, uint32_t flags) {
   // Reject illegal permissions
   if ((flags & PTE_W) && (flags & PTE_X)) {
      error("Security violation: Attempt to map memory as W^X simultaneously");
      return -ERR_SECURITY_VIOLATION;
   }
   // Proceed with safe mapping
   ...
}
```
- **Code pages**: Mapped as `PTE_R | PTE_X` (User or Supervisor).
- **Data / Heap / Stack**: Mapped as `PTE_R | PTE_W`.
- **MMIO Registers**: Mapped as `PTE_R | PTE_W` (User device driver pages).

### Privilege Ring Isolation
- Kernel pages never include the `PTE_U` flag. Any unprivileged user mode instruction attempting to read, write, or execute supervisor pages generates an immediate page fault exception (`CAUSE_FETCH_PAGE_FAULT` or `CAUSE_LOAD_PAGE_FAULT`).
- User processes operate with isolated page tables; one user process cannot map or view pages belonging to another process.

### Hardware PMP (Physical Memory Protection)
On RISC-V platforms, early boot code (`assembly/boot.S`) programs Machine-Mode PMP registers (`pmpcfg0`, `pmpaddr0`):
- Locks down supervisor and unprivileged access to Machine-mode security enclaves and firmware.
- Restricts unprivileged MMIO access strictly to regions explicitly granted through `pkcall_map_device()`.
