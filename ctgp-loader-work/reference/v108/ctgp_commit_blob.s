@ Position-independent CTGP-local GetSystemInfo compatibility wrapper.
@ Copied to a permanent game mapping, outside the runtime/BSS/snapshot ranges.
.section .pluginrodata_ctgp.v120,"a",%progbits
.arm
.balign 4
.global g_ctgpCommitBlob
.type g_ctgpCommitBlob, %object
g_ctgpCommitBlob:
    push    {r4, r5}
    mrs     r4, cpsr
    cmp     r1, #0x10000
    cmpeq   r2, #1
    bne     1f
    msr     cpsr_f, r4
    pop     {r4, r5}
    mov     r3, r0
    ldr     r1, 2f
    mov     r2, #0
    str     r1, [r3]
    str     r2, [r3, #4]
    mov     r0, #0
    bx      lr
1:
    msr     cpsr_f, r4
    pop     {r4, r5}
    push    {r0}
    svc     #0x2A
    pop     {r3}
    str     r1, [r3]
    str     r2, [r3, #4]
    bx      lr
2:
    .word   0xB3282131
.size g_ctgpCommitBlob, .-g_ctgpCommitBlob
