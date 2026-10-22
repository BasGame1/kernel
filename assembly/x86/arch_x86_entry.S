.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .note.pvh, "a"
.align 4
.long 4
.long 4
.long 18
.asciz "Xen"
.long _start_x86
.align 4

.section .text
.global _start_x86
.type _start_x86, @function

#if defined(__x86_64__) || defined(__64BIT__)
.code32
_start_x86:
   cli
   mov %eax, multiboot_magic
   mov %ebx, multiboot_info_ptr
   mov $pml4_table, %edi
   xor %eax, %eax
   mov $5120, %ecx
   cld
   rep stosl

   mov $pml4_table, %edi
   mov $pdpt_table, %eax
   or $0x7, %eax
   mov %eax, (%edi)

   mov $pdpt_table, %edi
   mov $pd_table, %eax
   or $0x7, %eax
   mov %eax, (%edi)

   mov $pd_table, %edi
   mov $0x83, %eax
   mov %eax, (%edi)
   add $8, %edi
   mov $0x200083, %eax
   mov $511, %ecx
1:
   mov %eax, (%edi)
   add $0x200000, %eax
   add $8, %edi
   loop 1b

   mov $pml4_table, %edi
   mov $kpdpt_table, %eax
   or $0x7, %eax
   mov %eax, 2048(%edi)

   mov $kpdpt_table, %edi
   mov $kpd_table, %eax
   or $0x7, %eax
   mov %eax, (%edi)

   mov $kpd_table, %edi
   mov $0x83, %eax
   mov %eax, (%edi)
   add $8, %edi
   mov $0x200083, %eax
   mov $511, %ecx
2:
   mov %eax, (%edi)
   add $0x200000, %eax
   add $8, %edi
   loop 2b

   mov %cr4, %eax
   or $(1 << 5), %eax
   mov %eax, %cr4

   mov $pml4_table, %eax
   mov %eax, %cr3

   mov $0xC0000080, %ecx
   rdmsr
   or $0x101, %eax
   wrmsr

   mov %cr0, %eax
   or $(1 << 31), %eax
   mov %eax, %cr0

   lgdt (gdt64_ptr)
   ljmp $0x08, $long_mode_start

.code64
long_mode_start:
   mov $0x10, %ax
   mov %ax, %ds
   mov %ax, %es
   mov %ax, %fs
   mov %ax, %gs
   mov %ax, %ss
   mov $stack_top_x86, %rsp

   mov %cr0, %rax
   and $~0xC, %rax
   or $0x2, %rax
   mov %rax, %cr0

   mov %cr4, %rax
   or $0x600, %rax
   mov %rax, %cr4

   mov $1, %eax
   cpuid
   bt $26, %ecx
   jnc 1f
   mov %cr4, %rax
   or $0x40000, %rax
   mov %rax, %cr4
   bt $28, %ecx
   jnc 1f
   xor %ecx, %ecx
   xgetbv
   or $7, %eax
   xsetbv
1:

   fninit

   mov $tss64, %rax
   movq $stack_top_x86, tss64+4(%rip)
   movw $104, tss64+102(%rip)
   movw $0x67, gdt64+48(%rip)
   movw %ax, gdt64+50(%rip)
   shr $16, %rax
   movb %al, gdt64+52(%rip)
   movb $0x89, gdt64+53(%rip)
   movb $0, gdt64+54(%rip)
   shr $8, %rax
   movb %al, gdt64+55(%rip)
   shr $8, %rax
   movl %eax, gdt64+56(%rip)
   movl $0, gdt64+60(%rip)

   mov $0x30, %ax
   ltr %ax

   call setup_idt

   mov $0xC0000081, %ecx
   xor %eax, %eax
   mov $0x00180008, %edx
   wrmsr

   mov $0xC0000082, %ecx
   mov $x86_syscall_entry, %rax
   mov %rax, %rdx
   shr $32, %rdx
   wrmsr

   mov $0xC0000084, %ecx
   mov $0x4700, %eax
   xor %edx, %edx
   wrmsr

   call kernel_init
2:
   hlt
   jmp 2b

setup_idt:
   mov $idt64, %rdi
   mov $256, %rcx
1:
   mov $isr_common_stub, %rax
   mov %ax, (%rdi)
   movw $0x08, 2(%rdi)
   movb $0, 4(%rdi)
   movb $0x8E, 5(%rdi)
   shr $16, %rax
   mov %ax, 6(%rdi)
   shr $16, %rax
   mov %eax, 8(%rdi)
   movl $0, 12(%rdi)
   add $16, %rdi
   loop 1b

   mov $idt64, %rdi
   add $(0x80 * 16), %rdi
   mov $x86_int80_entry, %rax
   mov %ax, (%rdi)
   movw $0x08, 2(%rdi)
   movb $0, 4(%rdi)
   movb $0xEE, 5(%rdi)
   shr $16, %rax
   mov %ax, 6(%rdi)
   shr $16, %rax
   mov %eax, 8(%rdi)
   movl $0, 12(%rdi)

   lidt (idt64_ptr)
   ret

.global x86_int80_entry
.type x86_int80_entry, @function
x86_int80_entry:
   cld
   push %rax
   push %rcx
   push %rdx
   push %rbx
   push %rbp
   push %rsi
   push %rdi
   push %r8
   push %r9
   push %r10
   push %r11
   push %r12
   push %r13
   push %r14
   push %r15

   mov %rdx, %r8
   mov %rcx, %rdx
   mov %r8, %rcx
   mov %rbx, %rsi
   mov %rax, %rdi
   call x86_kcall_handler
   mov %rax, 112(%rsp)

   pop %r15
   pop %r14
   pop %r13
   pop %r12
   pop %r11
   pop %r10
   pop %r9
   pop %r8
   pop %rdi
   pop %rsi
   pop %rbp
   pop %rbx
   pop %rdx
   pop %rcx
   pop %rax
   iretq

.global x86_syscall_entry
.type x86_syscall_entry, @function
x86_syscall_entry:
   cld
   mov %rsp, user_rsp_save(%rip)
   mov $kernel_syscall_stack_top, %rsp
   push %rcx
   push %r11
   push %r15
   push %r14
   push %r13
   push %r12
   push %rbp
   push %rbx
   push %r10
   push %r9
   push %r8
   push %rdi
   push %rsi
   push %rdx

   sub $8, %rsp
   push %r9
   mov %r8, %r9
   mov %r10, %r8
   mov %rdx, %rcx
   mov %rsi, %rdx
   mov %rdi, %rsi
   mov %rax, %rdi
   call antartica_x86_syscall
   add $16, %rsp

   mov 104(%rsp), %rdx
   shr $47, %rdx
   jnz 3f

   pop %rdx
   pop %rsi
   pop %rdi
   pop %r8
   pop %r9
   pop %r10
   pop %rbx
   pop %rbp
   pop %r12
   pop %r13
   pop %r14
   pop %r15
   pop %r11
   pop %rcx
   mov user_rsp_save(%rip), %rsp
   sysretq
3:
   mov $syscall_fault_msg, %rdi
   call fatal

.global isr_common_stub
.global trap_vector
.set trap_vector, isr_common_stub
isr_common_stub:
   cld
   push %rax
   push %rcx
   push %rdx
   push %rbx
   push %rbp
   push %rsi
   push %rdi
   push %r8
   push %r9
   push %r10
   push %r11
   push %r12
   push %r13
   push %r14
   push %r15
   mov %rsp, %rdi
   call trap_handler
   pop %r15
   pop %r14
   pop %r13
   pop %r12
   pop %r11
   pop %r10
   pop %r9
   pop %r8
   pop %rdi
   pop %rsi
   pop %rbp
   pop %rbx
   pop %rdx
   pop %rcx
   pop %rax
   iretq

.section .data
syscall_fault_msg:
   .asciz "Syscall RIP out of range"
.align 8
.global multiboot_magic
.global multiboot_info_ptr
multiboot_magic:
   .long 0
   .long 0
multiboot_info_ptr:
   .long 0
   .long 0
gdt64:
   .quad 0x0000000000000000
   .quad 0x00AF9A000000FFFF
   .quad 0x00CF92000000FFFF
   .quad 0x00CFFA000000FFFF
   .quad 0x00CFF2000000FFFF
   .quad 0x00AFFA000000FFFF
   .quad 0x0000000000000000
   .quad 0x0000000000000000
gdt64_end:

gdt64_ptr:
   .word gdt64_end - gdt64 - 1
   .long gdt64

.align 8
idt64_ptr:
   .word (256 * 16) - 1
   .quad idt64

#else

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
   iret
#endif

.section .bss
.align 4096
.global pml4_table
.global pdpt_table
.global pd_table
.global kpdpt_table
.global kpd_table
pml4_table:
   .skip 4096
pdpt_table:
   .skip 4096
pd_table:
   .skip 4096
kpdpt_table:
   .skip 4096
kpd_table:
   .skip 4096

.align 16
stack_bottom_x86:
   .skip 8192
stack_top_x86:

user_rsp_save:
   .quad 0

.align 16
kernel_syscall_stack:
   .skip 8192
kernel_syscall_stack_top:

.align 16
tss64:
   .skip 104

.align 16
idt64:
   .skip 4096
