    .global _enteruserspace
    .section .text

_enteruserspace:
   #if defined(__ARM__) || defined(__arm__) || defined(__aarch64__)
       #if defined(__ARM__)
          #if not defined(__32BIT__)
              msr sp_el0, x0
				  mov x1, #0
				  mov x2, #0
				  mov x3, #0
				  mov x4, #0
				  mov x5, #0
				  mov x6, #0
				  mov x7, #0
				  mov x8, #0
				  mov x9, #0
				  mov x10, #0
				  mov x11, #0
				  mov x12, #0
				  mov x13, #0
				  mov x14, #0
				  mov x15, #0
				  mov x16, #0
				  mov x17, #0
				  mov x18, #0
				  mov x19, #0
				  mov x20, #0
				  mov x21, #0
				  mov x22, #0
				  mov x23, #0
				  mov x24, #0
				  mov x25, #0
				  mov x26, #0
				  mov x27, #0
				  mov x28, #0
				  mov x29, #0
				  mov x30, #0
				  mov x0, #0
              eret
           #else
				  mov sp, r0
				  mov r1, #0
				  mov r2, #0
				  mov r3, #0
				  mov r4, #0
				  mov r5, #0
				  mov r6, #0
				  mov r7, #0
				  mov r8, #0
				  mov r9, #0
				  mov r10, #0
				  mov r11, #0
				  mov r12, #0
				  mov lr, #0
				  mov r0, #0
				  cps #0x10
				  bx lr
          #endif
      #endif
    #elif defined(__X86__)
       #if not defined(__32BIT__)
		     mov %rdi, %rsp
		     xor %rax, %rax
		     xor %rbx, %rbx
		     xor %rcx, %rcx
		     xor %rdx, %rdx
		     xor %rsi, %rsi
		     xor %rdi, %rdi
		     xor %rbp, %rbp
		     xor %r8, %r8
		     xor %r9, %r9
		     xor %r10, %r10
		     xor %r11, %r11
		     xor %r12, %r12
		     xor %r13, %r13
		     xor %r14, %r14
		     xor %r15, %r15
		     ret
       #else
		     mov 4(%esp), %eax
		     mov %eax, %esp
		     xor %eax, %eax
		     xor %ebx, %ebx
		     xor %ecx, %ecx
		     xor %edx, %edx
		     xor %esi, %esi
		     xor %edi, %edi
		     xor %ebp, %ebp
		     ret
    #endif
    
  #else
        mv sp, a0
        li ra, 0
        li gp, 0
        li tp, 0
        li t0, 0
        li t1, 0
        li t2, 0
        li s0, 0
        li s1, 0
        li a1, 0
        li a2, 0
        li a3, 0
        li a4, 0
        li a5, 0
        li a6, 0
        li a7, 0
        li s2, 0
        li s3, 0
        li s4, 0
        li s5, 0
        li s6, 0
        li s7, 0
        li s8, 0
        li s9, 0
        li s10, 0
        li s11, 0
        li t3, 0
        li t4, 0
        li t5, 0
        li t6, 0
        li a0, 0
        mret
  #endif
