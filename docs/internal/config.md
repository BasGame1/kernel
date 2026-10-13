# Kernel Configuration & Build Architecture

## Build System Overview
The kernel features a modular configuration system enabling fine-grained control over target CPU architectures, register bit widths, and optional subsystem runtimes. 

The build pipeline translates declarative `.kcfg` configuration files into C preprocessor definitions stored in `include/gen/config.h` 

---

## Configuration Flags & Macros

### Multiarch
The kernel compiles for arm32/64, x86_32/64 and riscv32/64
Default is riscv32, but for compiling other arch use the config
`X86=1` // force x86
`RISCV=1` // force riscv
`ARM=1` // force arm
`32BIT=1` // force 32bit

---

### Linux Kernel Module Support
- `__USE_LINUX_MODULES__`:
  - When **defined**: Builds the dual-mode 32/64-bit ELF `.ko` loader, exported KMI symbol dictionary, fast dispatch table, and the userspace module manager service (`LINUX_MODULE_GID = 16`).
  - When **undefined**: Entire subsystem is completely eliminated from the build. Inlined stub headers return `-1` or `NULL`, ensuring zero binary size or memory overhead.

---

## Configuration Profiles
There are default profiles for every arch on `config/`:
- `config/qemu-riscv`: Default RISC-V 32-bit QEMU virt profile.
- `config/default-riscv64`: RISC-V 64-bit multi-core profile.
- `config/default-x86`: 32-bit PC BIOS profile.
- `config/default-x86_64`: 64-bit PC UEFI/Multiboot profile.
- `config/default-arm`: 32-bit ARM Cortex-A profile.
- `config/default-arm64`: 64-bit ARMv8-A profile.

### Generating Configuration
If the default profile doesnt work due to making changes to the tree, you can automatically generate the config via the `.kcfg` files
```bash
./scripts/kcfg_parser.sh
```

And when you have the config file use create_config for making the header file

```bash
# For files under config/gen use gen/ prefix to the name
./scripts/create_config.sh <file name>
```
