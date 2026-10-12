    . global _start
    . section .text.init

_start: 
   la t0, _end
   la sp, stack_top
   li t0, 0xFFFFFFFF
   csrw pmpaddr0, t0
   li t1, 0x0F
   csrw pmpcfg0, t1
   call kernel_init
1:
   wfi
   j 1b
    
    .section .bss
    .align 4
    stack_bottom:
        .skip 4096
    stack_top:

