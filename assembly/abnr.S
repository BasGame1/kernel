	.global _start_live_kernel
	.section .text
_start_live_kernel:
li t0, 0x80200000
csrw mepc, t0 
li t1, (1 << 11)
csrs mstatus, t1
mret
; This is to boot the secondary kernel on ab no reboot updates
