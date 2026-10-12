#include "../include/drivers.h"
#include "../include/hardware.h"
#include "../include/kernel.h"
#include "../include/gid.h"

void drivers_init(void) {
   infon("Probing hardware peripherals");
#if defined(__ARM__)
   pl011_init(SCREEN_ADDRESS);
   successn("ARM PL011 driver initialized");
#elif defined(__X86__)
   uart_8250_init(COM1_PORT, 115200, 115200);
   successn("x86 8250 UART driver initialized");
#else
   uart_8250_init(SCREEN_ADDRESS, 3686400, 115200);
   successn("16550A UART driver initialized");
#endif
   #ifdef __USE_ETHERNET_E1000__
      e1000_init(0x40000000, 0);
   #endif
   #ifdef __USE_ETHERNET_RTL8139__
      rtl8139_init(0x40001000, 0);
   #endif
   #ifdef __USE_ETHERNET_VIRTIO__
      virtio_net_init(0x10002000, 0);
   #endif
   #ifdef __USE_WIFI_RTL8188EU__
      wifi_rtl8188eu_init(0x40002000, 0);
   #endif
   #ifdef __USE_WIFI_ATH9K__
      wifi_ath9k_init(0x40003000, 0);
   #endif
   #ifdef __USE_BLUETOOTH_BTUSB__
      bt_usb_init(0x40004000);
   #endif
   #ifdef __USE_BLUETOOTH_UART__
      bt_uart_init(0x10000000, 115200);
   #endif
   #ifdef __USE_NVME__
      nvme_init(0x50000000);
   #endif
   #ifdef __USE_GPU_VIRTIO__
      gpu_virtio_init(0x10003000);
   #endif
   #ifdef __USE_GPU_BOCHS__
      gpu_bochs_init(0x60000000, 1024, 768, 32);
   #endif
   #ifdef __USE_FB_SIMPLE__
      fb_simple_init(0x70000000, 1280, 720, 1280 * 4);
   #endif
   #ifdef __USE_AUDIO_INTEL_HDA__
      audio_intel_hda_init(0x80000000);
   #endif
   #ifdef __USE_AUDIO_VIRTIO__
      audio_virtio_init(0x10004000);
   #endif
   #ifdef __USE_AUDIO_AC97__
      audio_ac97_init(0x80001000, 0x80002000);
   #endif
   #ifdef __USE_SCREEN_GOLDFISH__
      screen_goldfish_init(0x90000000, 0x90100000);
   #endif
   #ifdef __USE_SCREEN_SSD1306__
      screen_ssd1306_init(0x90001000, 0x3C);
   #endif
   #ifdef __USE_USB_XHCI__
      usb_xhci_init(0xA0000000);
   #endif
   #ifdef __USE_USB_EHCI__
      usb_ehci_init(0xA0001000);
   #endif
   #ifdef __USE_USB_HID__
      usb_hid_init(0xA0002000);
   #endif
   #ifdef __USE_EXT4__
      init_ext4();
   #endif
   #ifdef __USE_NTFS__
      init_ntfs();
   #endif
   #ifdef __USE_VFAT__
      init_vfat();
   #endif
   #ifdef __USE_BTRFS__
      init_btrfs();
   #endif
   #ifdef __USE_ISO9660__
      init_iso9660();
   #endif
   #ifdef __USE_EXFAT__
      init_exfat();
   #endif
   #ifdef __USE_GPU_NOUVEAU__
      gpu_nouveau_init(0xB0000000, 0xC0000000);
   #endif
   #ifdef __USE_GPU_AMDGPU__
      gpu_amdgpu_init(0xD0000000, 0xE0000000);
   #endif
   #ifdef __USE_GPU_I915__
      gpu_i915_init(0xF0000000, 0xF1000000);
   #endif
   #ifdef __USE_KBD_I8042__
      kbd_i8042_init(0x10001000);
   #endif
   #ifdef __USE_KBD_PL050__
      kbd_pl050_init(0x10006000);
   #endif
   #ifdef __USE_KBD_GOLDFISH__
      kbd_goldfish_init(0x10007000);
   #endif
   #ifdef __USE_GPU_MSM_ADRENO__
      gpu_msm_adreno_init(0x50000000, 0x06030000);
   #endif
   #ifdef __USE_SERIAL_QCOM_GENI__
      serial_qcom_geni_init(0x50001000, 115200);
   #endif
   #ifdef __USE_SOC_QCOM_SMD__
      soc_qcom_smd_init(0x50100000);
   #endif
   #ifdef __USE_OVERLAYFS__
      init_overlayfs();
   #endif
   #ifdef __USE_EROFS__
      init_erofs();
   #endif
   #ifdef __USE_FIREWIRE__
      firewire_init(0xB1000000);
   #endif
   #ifdef __USE_NFC__
      nfc_init(0xB2000000);
   #endif
   #ifdef __USE_CAN__
      can_flexcan_init(0xB3000000, 500000);
   #endif
   #ifdef __USE_CELLULAR_RMNET__
      cellular_rmnet_init(1);
   #endif
   #ifdef __USE_NET_ENA__
      net_ena_init(0xC1000000, 0);
   #endif
   #ifdef __USE_NET_GVE__
      net_gve_init(0xC2000000, 0);
   #endif
   #ifdef __USE_NET_IXGBE__
      net_ixgbe_init(0xC3000000, 0);
   #endif
   #ifdef __USE_NET_MLX5__
      net_mlx5_init(0xC4000000);
   #endif
   #ifdef __USE_NET_STMMAC__
      net_stmmac_init(0xC5000000, 0);
   #endif
   #ifdef __USE_WIFI_IPW2200__
      wifi_ipw2200_init(0xC6000000, 0);
   #endif
   
}// drivers_init
