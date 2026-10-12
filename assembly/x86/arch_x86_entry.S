.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .text
.global _start_x86
.type _start_x86, @function

_start_x86:
   mov $stack_top_x86, %esp
   pushl $0
   popf
   call kernel_init
1:
   hlt
   jmp 1b

.global isr_common_stub
.global trap_vector
.set trap_vector, isr_common_stub
isr_common_stub:
   pusha
   mov %ds, %ax
   push %eax
   mov $0x10, %ax
   mov %ax, %ds
   mov %ax, %es
   mov %ax, %fs
   mov %ax, %gs
   push %esp
   call trap_handler
   add $4, %esp
   pop %eax
   mov %ax, %ds
   mov %ax, %es
   mov %ax, %fs
   mov %ax, %gs
   popa
   add $8, %esp
   iret

.section .bss
.align 16
stack_bottom_x86:
   .skip 8192
stack_top_x86:
