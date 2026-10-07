    . global _start
    . section .text.bios

_start: 
   la t0, _end
   call kernel_init
