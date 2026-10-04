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

.section .text.svcCopyHandle, "ax", %progbits
.global svcCopyHandle
.type svcCopyHandle, %function
svcCopyHandle:
    str r0, [sp, #-4]!
    svc 0xB1
    ldr r2, [sp], #4
    str r1, [r2]
    bx lr

.section .text.svcControlProcess, "ax", %progbits
.global svcControlProcess
.type svcControlProcess, %function
svcControlProcess:
    svc 0xB3
    bx lr
.section .text.LoaderDiag_Fatal, "ax", %progbits
.global LoaderDiag_Fatal
.type LoaderDiag_Fatal, %function
LoaderDiag_Fatal:
    ldr r2, .LdiagFatalMagic
    udf #0
    b .
.LdiagFatalMagic:
    .word 0x4C445844
/*
 * Y-armed pre-3GX gate. This block is copied verbatim into a Loader-owned
 * page, then that page is mapped into the target application. Rosalina's
 * stock gamePatchFunc has only its 0x07000100 literal temporarily replaced
 * with the mapped VA, so its original BLX r5 enters here before any 3GX
 * instruction executes.
 *
 * Control words are part of the copied block:
 *   +0 active  (game writes 1 on entry, 2 immediately before release)
 *   +4 release (Loader writes 1 only after Rosalina hooks are restored)
 */
.section .text.LoaderDiag_TrapStub, "ax", %progbits
.balign 4
.global LoaderDiag_TrapStubStart
.global LoaderDiag_TrapStubEnd
.global LoaderDiag_TrapControl
.type LoaderDiag_TrapStubStart, %function
LoaderDiag_TrapStubStart:
    push {r0-r12, lr}
    mrs  r0, cpsr
    push {r0}
    adr  r4, LoaderDiag_TrapControl
    mov  r0, #1
    str  r0, [r4, #0]
    svc  0x92                    @ flush entire data cache
1:
    ldr  r0, [r4, #4]
    cmp  r0, #0
    bne  2f
    ldr  r0, =1000000           @ 1 ms, low word of s64
    mov  r1, #0
    svc  0x0A                    @ svcSleepThread
    b    1b
2:
    mov  r0, #2
    str  r0, [r4, #0]
    svc  0x92
    pop  {r0}
    msr  cpsr_f, r0
    pop  {r0-r12, lr}
    ldr  r5, =0x07000100
    bx   r5                      @ preserve original gamePatchFunc LR
    .ltorg
    .balign 4
LoaderDiag_TrapControl:
    .word 0                      @ active/state
    .word 0                      @ release
    .word 0x50525459             @ 'YTRP'
    .word 0
LoaderDiag_TrapStubEnd:
.size LoaderDiag_TrapStubStart, .-LoaderDiag_TrapStubStart

/*
 * ZL-controlled two-stage CTGP gate.
 *
 * ZL itself patches CTGP's stable 0x07000130 mid-load point while Mario Kart
 * is held by the independent Y gate. That hook jumps to LateInstallerStart.
 * The installer immediately restores the two original 0x07000130 words,
 * derives CTGP's transient unpack footer from the 0x07000128 literal, finds
 * the E8BDDFFF epilogue near base+0x40, and replaces that one word with a B to
 * LateTrapEntry. Rosalina is never involved in this path.
 *
 * Control words:
 *   +0  late trap state (0=not entered, 1=held, 2=departing)
 *   +4  late release flag
 *   +8  magic 'ZTRP'
 *   +12 installer state (0=pending, 1=late hook installed, E1=no epilogue)
 *   +16 saved 0x07000130 word
 *   +20 saved 0x07000134 word
 *   +24 discovered late-site VA
 *   +28 installed late branch word
 */
.section .text.LoaderDiag_LateTrapStub, "ax", %progbits
.balign 4
.global LoaderDiag_LateTrapStubStart
.global LoaderDiag_LateTrapStubEnd
.global LoaderDiag_LateInstallerStart
.global LoaderDiag_LateTrapEntry
.global LoaderDiag_LateTrapControl
.type LoaderDiag_LateTrapStubStart, %function
LoaderDiag_LateTrapStubStart:
LoaderDiag_LateInstallerStart:
    push {r0-r12, lr}
    mrs  r12, cpsr
    push {r12}
    adr  r10, LoaderDiag_LateTrapControl

    @ Self-restore the stable 0x07000130 installer site first.
    ldr  r0, =0x07000130
    ldr  r1, [r10, #16]
    str  r1, [r0]
    ldr  r1, [r10, #20]
    str  r1, [r0, #4]
    svc  0x92
    svc  0x94

    @ Resolve the runtime base through stock CTGP's 0x07000128 ARM literal.
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

    @ Historical DUMP path searched from base+0x40. Search a small bounded
    @ window for the exact pop {r0-r12,lr,pc} epilogue.
    mov  r4, #9                  @ +0x40 .. +0x60 inclusive
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

    @ Encode ARM B from the discovered epilogue to our mapped late trap entry.
    adr  r5, LoaderDiag_LateTrapEntry
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
    ldr  pc, =0x07000130        @ execute the restored original stream

    .ltorg
    .balign 4
LoaderDiag_LateTrapEntry:
    mrs  r0, cpsr
    push {r0}
    adr  r4, LoaderDiag_LateTrapControl
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
    .word 0xE8BDDFFF              @ original pop {r0-r12,lr,pc}

    .ltorg
    .balign 4
LoaderDiag_LateTrapControl:
    .word 0                      @ late state
    .word 0                      @ release
    .word 0x5052545A             @ 'ZTRP'
    .word 0                      @ installer state
    .word 0                      @ saved 0x07000130
    .word 0                      @ saved 0x07000134
    .word 0                      @ late-site VA
    .word 0                      @ installed late branch
LoaderDiag_LateTrapStubEnd:
.size LoaderDiag_LateTrapStubStart, .-LoaderDiag_LateTrapStubStart
