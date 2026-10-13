.section .text.init
.global _start_arm
.type _start_arm, @function

#if defined(__aarch64__) || defined(__64BIT__)
_start_arm:
   adrp x0, stack_top_arm
   add sp, x0, :lo12:stack_top_arm
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
   b arm_sync_handler
   b arm_irq_handler
   b arm_fiq_handler

arm_sync_handler:
   sub sp, sp, #256
   stp x0, x1, [sp, #0]
   stp x2, x3, [sp, #16]
   stp x4, x5, [sp, #32]
   stp x6, x7, [sp, #48]
   stp x8, x9, [sp, #64]
   stp x10, x11, [sp, #80]
   stp x12, x13, [sp, #96]
   stp x14, x15, [sp, #112]
   stp x16, x17, [sp, #128]
   stp x18, x19, [sp, #144]
   stp x20, x21, [sp, #160]
   stp x22, x23, [sp, #176]
   stp x24, x25, [sp, #192]
   stp x26, x27, [sp, #208]
   stp x28, x29, [sp, #224]
   str x30, [sp, #240]
   mov x0, sp
   bl trap_handler
   ldr x30, [sp, #240]
   ldp x28, x29, [sp, #224]
   ldp x26, x27, [sp, #208]
   ldp x24, x25, [sp, #192]
   ldp x22, x23, [sp, #176]
   ldp x20, x21, [sp, #160]
   ldp x18, x19, [sp, #144]
   ldp x16, x17, [sp, #128]
   ldp x14, x15, [sp, #112]
   ldp x12, x13, [sp, #96]
   ldp x10, x11, [sp, #80]
   ldp x8, x9, [sp, #64]
   ldp x6, x7, [sp, #48]
   ldp x4, x5, [sp, #32]
   ldp x2, x3, [sp, #16]
   ldp x0, x1, [sp, #0]
   add sp, sp, #256
   eret

arm_irq_handler:
   b arm_sync_handler

arm_fiq_handler:
   b .

#else

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

#endif

.section .bss
.align 4
stack_bottom_arm:
   .skip 4096
stack_top_arm:
