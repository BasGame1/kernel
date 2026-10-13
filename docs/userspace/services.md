# Userspace Microkernel Services

## Service Registry
- The GID registry goes as follow

| Service Name | GID | Entry Function | Required Privileges | Description |
|---|---|---|---|---|
| **Console Logger** | `0` (`PRINT_GID`) | `print_process` | None | System message collection and serial log demuxing |
| **Screen Manager** | `1` (`SCREEN_GID`) | `start_screen` | `PKCALL_MAP_DEVICE` | Framebuffer abstraction (SSD1306, Goldfish FB, Bochs) |
| **Keyboard Input** | `2` (`KEYBOARD_GID`)| `keyboard_process` | `PKCALL_MAP_DEVICE` | Keystroke capture (i8042, Goldfish KBD, PL050) |
| **VirtIO Bus** | `3` (`VIRTIO_GID`) | `virtio_process` | `PKCALL_MAP_DEVICE` | VirtIO transport layer (MMIO & PCI enumeration) |
| **File Systems** | `4` (`FS_GID`) | `fs_process` | Block IO IPC | VFS manager dispatching IOFS, Ext2/4, FAT, EROFS |
| **Network Stack** | `5` (`NETWORK_GID`)| `net_process` | `PKCALL_MAP_DEVICE` | Packet driver (e1000, RTL8139, VirtIO-Net) |
| **Graphics Engine**| `6` (`GPU_GID`)| `gpu_process` | `PKCALL_MAP_DEVICE` | 2D/3D GPU driver (Adreno, AMDGPU, i915, Nouveau) |
| **Platform Bridge**| `7` (`HW_GID`)| `hw_process` | `PKCALL_MAP_DEVICE` | Raw platform configuration and chip interconnect |
| **A/B Updates** | `11` (`UPDATE_PROCESS_GID`) | `update_process` | `FS_GID` IPC | Dual-slot A/B non-rollback partition updates |
| **Antartica ABI** | `12` (`ANTARTICA_GID`) | `antartica_process` | Multi-service IPC | 467-entry Linux syscall emulation daemon |
| **Linux Modules** | `16` (`LINUX_MODULE_GID`)| `linux_module_service_main` | Dynamic heap | In-memory ELF `.ko` loader and KMI runtime |

> [!NOTE]
> GID 8 to 10 can be priviledged or not, selected via the config `DISABLE_GID810_PKCALLS`, these gids are placeholders in case more priviledged services must be added

