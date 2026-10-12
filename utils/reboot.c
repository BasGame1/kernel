#include "../include/kernel.h"

void kreboot() {
}// kreboot

void reboot() {
   infon("Rebooting device");
#if defined(__RISCV__) || defined(__riscv)
   asm volatile("jr %0" :: "r"(0x80000000));
#elif defined(__X86__) || defined(__i386__) || defined(__x86_64__)
   asm volatile("jmp *%0" :: "r"(0x00100000));
#elif defined(__ARM__) || defined(__arm__) || defined(__aarch64__)
   asm volatile("bx %0" :: "r"(0x40000000));
#else
   void (*reset_fn)(void) = (void (*)(void))0;
   reset_fn();
#endif
}// reboot

void shutdown() {
}// shutdown
