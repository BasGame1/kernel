    . global _start
    . section .text.bios

_start: 
   call kernelInit
       cli
   1:  hlt
       jmp 1b
   ; Enable Machine Interrput Enable
   mov mie 1
