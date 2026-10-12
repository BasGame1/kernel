.section .text.init
.global _start_arm
.type _start_arm, @function

_start_arm:
   ldr sp, =stack_top_arm
   bl kernel_init
1:
   wfi
   b 1b

.section .text
.align 4
.global arm_vector_table
.global trap_vector
.set trap_vector, arm_vector_table
arm_vector_table:
   b _start_arm
   b arm_undef_handler
   b arm_svc_handler
   b arm_pabort_handler
   b arm_dabort_handler
   b .
   b arm_irq_handler
   b arm_fiq_handler

arm_svc_handler:
   sub lr, lr, #4
   srsdb sp!, #0x13
   cps #0x13
   push {r0-r12}
   mov r0, sp
   bl trap_handler
   mov sp, r0
   pop {r0-r12}
   rfeia sp!

arm_undef_handler:
   b .

arm_pabort_handler:
   b .

arm_dabort_handler:
   b .

arm_irq_handler:
   sub lr, lr, #4
   srsdb sp!, #0x13
   cps #0x13
   push {r0-r12}
   mov r0, sp
   bl trap_handler
   mov sp, r0
   pop {r0-r12}
   rfeia sp!

arm_fiq_handler:
   b .

.section .bss
.align 4
stack_bottom_arm:
   .skip 4096
stack_top_arm:
