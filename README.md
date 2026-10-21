![Platform](https://img.shields.io/badge/Linux-grey?logo=linux)
![Static Badge](https://img.shields.io/badge/Bash-grey?logo=GNU%20Bash)
![Static Badge](https://badgen.net/badge/icon/Language?icon=C&label)

> [!WARNING]
> Readme still on todo

# Lyncex Kernel
## Written in C, Lyncex is a kernel focused on security and stupid features
### Features:
- **Neo**: A secure little ¿kernel? written in rust running in parallel to the kernel for verifying signatures, administrate SSH keys and catch malware
- **Antartica**: A compatibility layer between GNU/Linux and lyncex, that makes possible run GNU/Linux apps and even a whole GNU/Linux distro with Lyncex as the kernel
- **Microkernel**: As a microkernel architecture, any driver, process or system that hangs or fails will not affect the rest or make the whole kernel crash
- **No reboot updates**: Featuring A/B updates and ABNR updates, the kernel can update and reboot itself without making the underlying OS stop or reboot, ideal for server that need be online 24/7
- **Multiarch kernel**: Has building support for ARM, RISCV and X86 in 32 and 64 bit archs
- **Minimal**: The kernel core takes less than 1 minute to compile and less than 1 minute to boot due to the reduced code
- **Pkcalls**: Syscalls (kcalls), are divided in priviledged (pkcall) and not priviledged (kcall), so only selected processes can access certain kernel calls
- **GID allowlist**: Every process type (audio, screen, etc) has its own GID (group id), and cannot communicate with a process of other GID unless it is explicitely allowed in source code
- **SID verificartion**: Critical userspace components (screen, filesystem, etc), have the priviledge status, for verification, they need an SID (system id), which verifies the integrity of the files, if they are modified, the SID stops being valid and the process looses its priviledged status
- **User functions**: On userspace, non priviledged processes can only access a few basic calls, like `printu()`for printing in the screen, so kernel and system components use different functions
- **Malware catching**: Neo provides a public API for matching the digest of any file to a know malware collection
- **All times signature verification**: periodically Neo will match the kernel and its own digests against the ones on compilation, to see if the kernel has been altered
- **Kernel Modules**: Kernel modules are supported to easily load drivers or any additional software
