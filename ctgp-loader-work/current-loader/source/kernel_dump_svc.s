.text
.arm
.balign 4

/* Luma custom SVC 0x90: privileged VA -> PA translation. */
.global svcConvertVAToPA
.type svcConvertVAToPA, %function
svcConvertVAToPA:
    svc 0x90
    bx  lr

/*
 * Read four words through a privileged address without having kernel mode
 * write directly into Loader's user address space.
 *
 * User ABI:
 *   r0 = u32 out[4]
 *   r1 = privileged/direct-physical source VA
 *
 * CustomBackdoor (svc 0x80) invokes callback(r1, r2, r3). The callback
 * returns the four words in r0-r3. Once svc returns to user mode, this
 * wrapper stores them to out[].
 */
.global svcCustomBackdoorRead4
.type svcCustomBackdoorRead4, %function
svcCustomBackdoorRead4:
    push {r4, lr}
    mov  r4, r0
    ldr  r0, =LoaderDiag_KernelRead4Callback
    mov  r2, #0
    mov  r3, #0
    svc  0x80
    stmia r4, {r0-r3}
    mov  r0, #0
    pop  {r4, pc}

.type LoaderDiag_KernelRead4Callback, %function
LoaderDiag_KernelRead4Callback:
    mov  r12, r0
    ldmia r12, {r0-r3}
    bx   lr
