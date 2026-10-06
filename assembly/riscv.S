	.global _start
	.section .text
_start:
li t0, 0x80200000
csrw mepc, t0 
li t1, (1 << 11)
csrs mstatus, t1
mret
; This is an alternative try to boot the kernel as a backup
