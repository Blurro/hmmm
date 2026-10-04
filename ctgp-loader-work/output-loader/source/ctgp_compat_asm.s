.arm
.balign 4

.section .text.svcMapProcessMemoryEx, "ax", %progbits
.global svcMapProcessMemoryEx
.type svcMapProcessMemoryEx, %function
svcMapProcessMemoryEx:
    push {r4, r5, r6}
    ldr r4, [sp, #12]
    ldr r5, [sp, #16]
    mov r6, r0
    mov r0, #0xFFFFFFF2
    svc 0xA0
    pop {r4, r5, r6}
    bx lr

.section .text.svcUnmapProcessMemoryEx, "ax", %progbits
.global svcUnmapProcessMemoryEx
.type svcUnmapProcessMemoryEx, %function
svcUnmapProcessMemoryEx:
    svc 0xA1
    bx lr

.section .text.svcFlushEntireDataCache, "ax", %progbits
.global svcFlushEntireDataCache
.type svcFlushEntireDataCache, %function
svcFlushEntireDataCache:
    svc 0x92
    bx lr

.section .text.svcInvalidateEntireInstructionCache, "ax", %progbits
.global svcInvalidateEntireInstructionCache
.type svcInvalidateEntireInstructionCache, %function
svcInvalidateEntireInstructionCache:
    svc 0x94
    bx lr

/*
 * Automatic post-unpack rendezvous.
 *
 * Loader installs the 0x07000130 entry only after PLGLDR_LoadPlugin has
 * synchronously finished mapping the 3GX and before PM can start the process.
 * This installer self-restores 0x07000130, finds the known post-unpack
 * E8BDDFFF epilogue, and changes only that epilogue to enter the sleep gate.
 *
 * Control words:
 *   +0  late state: 0 not entered, 1 held, 2 departing
 *   +4  release: Loader writes 1 after both commit hooks are installed
 *   +8  magic 'ZTRP'
 *   +12 installer state: 0 pending, 1 installed, E1 no epilogue
 *   +16 saved 0x07000130
 *   +20 saved 0x07000134
 *   +24 discovered late epilogue VA
 *   +28 installed late branch
 *   +32 Loader compat state: 0 pending, 2 success, E* failure
 *   +36 Loader Result
 */
.section .text.CtgpCompat_LateTrapStub, "ax", %progbits
.balign 4
.global CtgpCompat_LateTrapStubStart
.global CtgpCompat_LateTrapStubEnd
.global CtgpCompat_LateInstallerStart
.global CtgpCompat_LateTrapEntry
.global CtgpCompat_LateTrapControl
.type CtgpCompat_LateTrapStubStart, %function
CtgpCompat_LateTrapStubStart:
CtgpCompat_LateInstallerStart:
    push {r0-r12, lr}
    mrs  r12, cpsr
    push {r12}
    adr  r10, CtgpCompat_LateTrapControl

    @ Restore the stable installer site before doing anything else.
    ldr  r0, =0x07000130
    ldr  r1, [r10, #16]
    str  r1, [r0]
    ldr  r1, [r10, #20]
    str  r1, [r0, #4]
    svc  0x92
    svc  0x94

    @ Resolve CTGP's transient unpack footer through the 0x07000128 literal.
    ldr  r0, =0x07000128
    ldr  r1, [r0]
    ldr  r2, =0x00000FFF
    and  r2, r1, r2
    add  r3, r0, #8
    tst  r1, #(1 << 23)
    addne r3, r3, r2
    subeq r3, r3, r2
    ldr  r3, [r3]
    add  r3, r3, #0x40

    @ Match the established DUMP-return epilogue in the bounded +0x40..+0x60
    @ window. The current CTGP build has exactly one E8BDDFFF here.
    mov  r4, #9
1:
    ldr  r5, [r3]
    ldr  r6, =0xE8BDDFFF
    cmp  r5, r6
    beq  2f
    add  r3, r3, #4
    subs r4, r4, #1
    bne  1b

    mov  r0, #0xE1
    str  r0, [r10, #12]
    svc  0x92
    b    4f

2:
    str  r3, [r10, #24]

    @ ARM B from the discovered epilogue to the mapped late gate.
    adr  r5, CtgpCompat_LateTrapEntry
    sub  r5, r5, r3
    sub  r5, r5, #8
    mov  r5, r5, asr #2
    ldr  r6, =0x00FFFFFF
    and  r5, r5, r6
    ldr  r6, =0xEA000000
    orr  r5, r5, r6
    str  r5, [r10, #28]
    str  r5, [r3]
    svc  0x92
    svc  0x94

    mov  r0, #1
    str  r0, [r10, #12]
    svc  0x92

4:
    pop  {r12}
    msr  cpsr_f, r12
    pop  {r0-r12, lr}
    ldr  pc, =0x07000130

    .ltorg
    .balign 4

CtgpCompat_LateTrapEntry:
    mrs  r0, cpsr
    push {r0}
    adr  r4, CtgpCompat_LateTrapControl
    mov  r0, #1
    str  r0, [r4, #0]
    svc  0x92
5:
    ldr  r0, [r4, #4]
    cmp  r0, #0
    bne  6f
    ldr  r0, =1000000
    mov  r1, #0
    svc  0x0A
    b    5b
6:
    mov  r0, #2
    str  r0, [r4, #0]
    svc  0x92
    pop  {r0}
    msr  cpsr_f, r0
    .word 0xE8BDDFFF

    .ltorg
    .balign 4
CtgpCompat_LateTrapControl:
    .word 0
    .word 0
    .word 0x5052545A
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
CtgpCompat_LateTrapStubEnd:
.size CtgpCompat_LateTrapStubStart, .-CtgpCompat_LateTrapStubStart

/*
 * Position-independent CTGP-local GetSystemInfo compatibility wrapper.
 * This is byte-for-byte the v1.08 r6/r7 23-word body, only with a Loader-local
 * symbol name. It permanently filters exactly type=0x10000,param=1.
 */
.section .rodata.CtgpCompat_CommitBlob, "a", %progbits
.arm
.balign 4
.global CtgpCompat_CommitBlob
.type CtgpCompat_CommitBlob, %object
CtgpCompat_CommitBlob:
    push    {r4, r5}
    mrs     r4, cpsr
    cmp     r1, #0x10000
    cmpeq   r2, #1
    bne     7f
    msr     cpsr_f, r4
    pop     {r4, r5}
    mov     r3, r0
    ldr     r1, 8f
    mov     r2, #0
    str     r1, [r3]
    str     r2, [r3, #4]
    mov     r0, #0
    bx      lr
7:
    msr     cpsr_f, r4
    pop     {r4, r5}
    push    {r0}
    svc     #0x2A
    pop     {r3}
    str     r1, [r3]
    str     r2, [r3, #4]
    bx      lr
8:
    .word   0xB3282131
.size CtgpCompat_CommitBlob, .-CtgpCompat_CommitBlob
