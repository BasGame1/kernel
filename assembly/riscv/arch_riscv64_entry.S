.section .text.init
.global _start64
.type _start64, @function
.global _start
.set _start, _start64

_start64:
   la sp, stack_top_64
   la t0, _end
   srli t0, t0, 2
   csrw pmpaddr0, t0
   li t0, -1
   csrw pmpaddr1, t0
   li t1, 0x0F08
   csrw pmpcfg0, t1
   call kernel_init
1:
   wfi
   j 1b

.section .text
.align 4
.global trap_vector_64
.type trap_vector_64, @function
.global trap_vector
.set trap_vector, trap_vector_64

trap_vector_64:
   csrrw sp, mscratch, sp
   bnez sp, 1f
   csrrw sp, mscratch, sp
1:
   addi sp, sp, -256
   sd x1, 8(sp)
   csrr x1, mepc
   sd x1, 0(sp)
   csrr x1, mscratch
   sd x1, 16(sp)
   sd x3, 24(sp)
   sd x4, 32(sp)
   sd x5, 40(sp)
   sd x6, 48(sp)
   sd x7, 56(sp)
   sd x8, 64(sp)
   sd x9, 72(sp)
   sd x10, 80(sp)
   sd x11, 88(sp)
   sd x12, 96(sp)
   sd x13, 104(sp)
   sd x14, 112(sp)
   sd x15, 120(sp)
   sd x16, 128(sp)
   sd x17, 136(sp)
   sd x18, 144(sp)
   sd x19, 152(sp)
   sd x20, 160(sp)
   sd x21, 168(sp)
   sd x22, 176(sp)
   sd x23, 184(sp)
   sd x24, 192(sp)
   sd x25, 200(sp)
   sd x26, 208(sp)
   sd x27, 216(sp)
   sd x28, 224(sp)
   sd x29, 232(sp)
   sd x30, 240(sp)
   sd x31, 248(sp)

   mv a0, sp
   call trap_handler
   mv sp, a0

   ld x1, 0(sp)
   csrw mepc, x1
   ld x1, 8(sp)
   ld x3, 24(sp)
   ld x4, 32(sp)
   ld x5, 40(sp)
   ld x6, 48(sp)
   ld x7, 56(sp)
   ld x8, 64(sp)
   ld x9, 72(sp)
   ld x10, 80(sp)
   ld x11, 88(sp)
   ld x12, 96(sp)
   ld x13, 104(sp)
   ld x14, 112(sp)
   ld x15, 120(sp)
   ld x16, 128(sp)
   ld x17, 136(sp)
   ld x18, 144(sp)
   ld x19, 152(sp)
   ld x20, 160(sp)
   ld x21, 168(sp)
   ld x22, 176(sp)
   ld x23, 184(sp)
   ld x24, 192(sp)
   ld x25, 200(sp)
   ld x26, 208(sp)
   ld x27, 216(sp)
   ld x28, 224(sp)
   ld x29, 232(sp)
   ld x30, 240(sp)
   ld x31, 248(sp)

   addi sp, sp, 256
   csrw mscratch, sp
   ld sp, -240(sp)
   mret

.section .bss
.align 8
stack_bottom_64:
   .skip 8192
stack_top_64:
