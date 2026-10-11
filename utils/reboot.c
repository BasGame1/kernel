#include "../include/kernel.h"

void kreboot() {
   // Reboots only the kernel
}// kreboot

void reboot() {
   infon("Rebooting device");
   asm volatile("jr %0" :: "r"(0x80000000));
}// reboot

void shutdown() {
   // Shuts down the device
}// shutdown
