    . global _start
    . section .text.init

_start: 
   la t0, _end
   la sp, stack_top
   call kernel_init
    
    
    .section .bss
    .align 4
    stack_bottom:
        .skip 4096
    stack_top:

