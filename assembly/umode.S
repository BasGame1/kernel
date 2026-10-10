    .global _enteruserspace
    .section .text

_enteruserspace:
    mv sp, a0
    mret
