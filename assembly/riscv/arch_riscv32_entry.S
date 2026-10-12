    . global _start
    . section .text.init

_start: 
   la sp, stack_top
   la t0, _end
   srli t0, t0, 2
   csrw pmpaddr0, t0
   li t0, 0xFFFFFFFF
   csrw pmpaddr1, t0
   li t1, 0x0F08
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

