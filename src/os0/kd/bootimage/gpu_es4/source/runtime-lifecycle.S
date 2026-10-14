/* Recovered runtime worker, interrupt and signal dispatch dependencies. */
.syntax unified
.thumb
.text
.align 2
.global GpuRuntimeStart
.type GpuRuntimeStart, %function
.thumb_func
GpuRuntimeStart:
	push {r3, r4, r5, r6, r7, lr}
	ldr r4, GpuRuntimeStartLiteral44
	ldr r0, [r4]
	bl GpuRuntimeStateSet
	mov r5, r0
	cbz r0, .LGpuRuntimeStart1c
	ldr r0, [r4]
	bl GpuRuntimeCleanup
	movs r3, #0
	mov r0, r5
	str r3, [r4]
	pop {r3, r4, r5, r6, r7, pc}
.LGpuRuntimeStart1c:
	ldr r7, GpuRuntimeStartLiteral48
	ldr r0, [r4]
	ldr r1, [r7, #0x24]
	bl GpuRuntimeDeviceAcquire
	mov r6, r0
	cbz r0, .LGpuRuntimeStart3a
	ldr r0, [r4]
	bl GpuRuntimeStateClear
	ldr r0, [r4]
	bl GpuRuntimeCleanup
	str r5, [r4]
	b .LGpuRuntimeStart40
.LGpuRuntimeStart3a:
	ldr r0, [r7, #0x24]
	bl ksceKernelEnableIntr
.LGpuRuntimeStart40:
	mov r0, r6
	pop {r3, r4, r5, r6, r7, pc}
.size GpuRuntimeStart, .-GpuRuntimeStart
.align 2
GpuRuntimeStartLiteral44:
	.word gpuDumpRegion + 0x3178
GpuRuntimeStartLiteral48:
	.word gpuDumpRegion + 0x313c
.align 2
.global GpuRuntimeCleanup
.type GpuRuntimeCleanup, %function
.thumb_func
GpuRuntimeCleanup:
	ldr r3, GpuRuntimeCleanupLiteral4c
	push {r4, r5, r6, lr}
	mov r4, r0
	ldr r0, [r3, #0x24]
	bl ksceKernelDisableIntr
	mov r0, r4
	bl GpuRuntimeDeviceRelease
	cbnz r0, .LGpuRuntimeCleanup48
	mov r0, r4
	bl GpuRuntimeStateClear
	cbnz r0, .LGpuRuntimeCleanup48
	ldr r3, GpuRuntimeCleanupLiteral50
	ldr r0, [r3]
	bl GpuUnregisterDevice
	cbnz r0, .LGpuRuntimeCleanup48
	ldr r5, GpuRuntimeCleanupLiteral54
	ldr r3, [r5]
	ldr r0, [r3, #0x38]
	bl GpuServiceBufferDestroy
	mov r4, r0
	cbnz r0, .LGpuRuntimeCleanup46
	ldr r6, [r5]
	mov r0, r6
	bl GpuProcessServicesDeinit
	add.w r0, r6, #0x24
	bl GpuServicePairDeinit
	str r4, [r5]
.LGpuRuntimeCleanup46:
	mov r0, r4
.LGpuRuntimeCleanup48:
	pop {r4, r5, r6, pc}
	nop 
.size GpuRuntimeCleanup, .-GpuRuntimeCleanup
.align 2
GpuRuntimeCleanupLiteral4c:
	.word gpuDumpRegion + 0x313c
GpuRuntimeCleanupLiteral50:
	.word gpuDumpRegion + 0x3134
GpuRuntimeCleanupLiteral54:
	.word gpuDumpRegion + 0x3178
#if FW_DEBUG
.align 2
.global GpuRuntimeDeviceAcquire
.type GpuRuntimeDeviceAcquire, %function
.thumb_func
GpuRuntimeDeviceAcquire:
	push.w {r4, r5, r6, r7, r8, sb, lr}
	sub sp, #0x1c
	ldr r3, GpuRuntimeDeviceAcquireLiteralc4
	mov r7, r0
	mov r6, r1
	ldr r5, [r0, #0x38]
	ldr r2, [r3]
	mov r8, r3
	ldr r4, [r5, #4]
	str r2, [sp, #0x14]
	str r5, [sp, #0x10]
	cmp r4, #0
	bne .LGpuRuntimeDeviceAcquireae
	mov.w sb, #0xf
	mov.w r3, #0x2000
	str r4, [sp]
	movs r2, #0x14
	strd sb, r4, [sp, #4]
	ldr r0, GpuRuntimeDeviceAcquireLiteralc8
	ldr r1, GpuRuntimeDeviceAcquireLiteralcc
	bl ksceKernelCreateThread
	ldr r3, [sp, #0x10]
	str r0, [r5, #0x14]
	ldr r3, [r3, #0x14]
	cmp r3, #0
	bge .LGpuRuntimeDeviceAcquire52
	ldr r2, GpuRuntimeDeviceAcquireLiterald0
	mov r1, r4
	movs r0, #1
	stm.w sp, {r2, r3}
	ldr r2, GpuRuntimeDeviceAcquireLiterald4
	ldr r3, GpuRuntimeDeviceAcquireLiterald8
	bl ksceKernelPrintfLevelWithInfo
	b .LGpuRuntimeDeviceAcquire98
.LGpuRuntimeDeviceAcquire52:
	movs r3, #0x50
	str r4, [sp, #0xc]
	strd r3, sb, [sp, #4]
	mov r2, r4
	str r7, [sp]
	mov r0, r6
	ldr r1, GpuRuntimeDeviceAcquireLiteraldc
	ldr r3, GpuRuntimeDeviceAcquireLiterale0
	bl ksceKernelRegisterIntrHandler
	ldr r4, [sp, #0x10]
	mov r5, r0
	cbz r0, .LGpuRuntimeDeviceAcquire72
	ldr r0, [r4, #0x14]
	b .LGpuRuntimeDeviceAcquire94
.LGpuRuntimeDeviceAcquire72:
	movs r3, #1
	mov r2, r5
	str r3, [r4, #4]
	mov r1, r5
	str r3, [r4, #0x1c]
	mov r3, r5
	strd r6, r7, [r4, #0xc]
	ldr r0, GpuRuntimeDeviceAcquireLiterale4
	bl ksceKernelCreateEventFlag
	ldr r3, [sp, #0x10]
	str r0, [r4, #0x18]
	ldrd r0, r2, [r3, #0x14]
	cmp r2, #0
	bge .LGpuRuntimeDeviceAcquire9c
.LGpuRuntimeDeviceAcquire94:
	bl ksceKernelDeleteThread
.LGpuRuntimeDeviceAcquire98:
	movs r0, #0x5e
	b .LGpuRuntimeDeviceAcquireb0
.LGpuRuntimeDeviceAcquire9c:
	movs r1, #4
	add r2, sp, #0x10
	bl ksceKernelStartThread
	mov r0, r6
	bl ksceKernelEnableIntr
	mov r0, r5
	b .LGpuRuntimeDeviceAcquireb0
.LGpuRuntimeDeviceAcquireae:
	movs r0, #0x60
.LGpuRuntimeDeviceAcquireb0:
	ldr r2, [sp, #0x14]
	ldr.w r3, [r8]
	cmp r2, r3
	beq .LGpuRuntimeDeviceAcquirebe
	bl __stack_chk_fail
.LGpuRuntimeDeviceAcquirebe:
	add sp, #0x1c
	pop.w {r4, r5, r6, r7, r8, sb, pc}
.size GpuRuntimeDeviceAcquire, .-GpuRuntimeDeviceAcquire
.align 2
GpuRuntimeDeviceAcquireLiteralc4:
	.word __stack_chk_guard
GpuRuntimeDeviceAcquireLiteralc8:
	.word gpuRuntimeThreadName
GpuRuntimeDeviceAcquireLiteralcc:
	.word GpuRuntimeWorker
GpuRuntimeDeviceAcquireLiterald0:
	.word gpuRuntimeInstallFunction
GpuRuntimeDeviceAcquireLiterald4:
	.word gpuRuntimeInstallInfo
GpuRuntimeDeviceAcquireLiterald8:
	.word gpuRuntimeThreadCreateFormat
GpuRuntimeDeviceAcquireLiteraldc:
	.word gpuRuntimeLisrName
GpuRuntimeDeviceAcquireLiterale0:
	.word GpuRuntimeInterrupt
GpuRuntimeDeviceAcquireLiterale4:
	.word gpuRuntimeIsrName
#else
.align 2
.global GpuRuntimeDeviceAcquire
.type GpuRuntimeDeviceAcquire, %function
.thumb_func
GpuRuntimeDeviceAcquire:
	push.w {r4, r5, r6, r7, r8, sb, lr}
	sub sp, #0x1c
	ldr r3, GpuRuntimeDeviceAcquireLiteralb4
	mov r7, r0
	mov r6, r1
	ldr r5, [r0, #0x38]
	ldr r2, [r3]
	mov r8, r3
	ldr r4, [r5, #4]
	str r2, [sp, #0x14]
	str r5, [sp, #0x10]
	cmp r4, #0
	bne .LGpuRuntimeDeviceAcquire98
	mov.w sb, #0xf
	mov.w r3, #0x2000
	str r4, [sp]
	movs r2, #0x14
	strd sb, r4, [sp, #4]
	ldr r0, GpuRuntimeDeviceAcquireLiteralb8
	ldr r1, GpuRuntimeDeviceAcquireLiteralbc
	bl ksceKernelCreateThread
	ldr r3, [sp, #0x10]
	str r0, [r5, #0x14]
	ldr r3, [r3, #0x14]
	cmp r3, #0
	blt .LGpuRuntimeDeviceAcquire9c
	movs r3, #0x50
	str r4, [sp, #0xc]
	strd r3, sb, [sp, #4]
	mov r2, r4
	str r7, [sp]
	mov r0, r6
	ldr r1, GpuRuntimeDeviceAcquireLiteralc0
	ldr r3, GpuRuntimeDeviceAcquireLiteralc4
	bl ksceKernelRegisterIntrHandler
	ldr r4, [sp, #0x10]
	mov r5, r0
	cbz r0, .LGpuRuntimeDeviceAcquire5e
	ldr r0, [r4, #0x14]
	b .LGpuRuntimeDeviceAcquire80
.LGpuRuntimeDeviceAcquire5e:
	movs r3, #1
	mov r2, r5
	str r3, [r4, #4]
	mov r1, r5
	str r3, [r4, #0x1c]
	mov r3, r5
	strd r6, r7, [r4, #0xc]
	ldr r0, GpuRuntimeDeviceAcquireLiteralc8
	bl ksceKernelCreateEventFlag
	ldr r3, [sp, #0x10]
	str r0, [r4, #0x18]
	ldrd r0, r2, [r3, #0x14]
	cmp r2, #0
	bge .LGpuRuntimeDeviceAcquire86
.LGpuRuntimeDeviceAcquire80:
	bl ksceKernelDeleteThread
	b .LGpuRuntimeDeviceAcquire9c
.LGpuRuntimeDeviceAcquire86:
	movs r1, #4
	add r2, sp, #0x10
	bl ksceKernelStartThread
	mov r0, r6
	bl ksceKernelEnableIntr
	mov r0, r5
	b .LGpuRuntimeDeviceAcquire9e
.LGpuRuntimeDeviceAcquire98:
	movs r0, #0x60
	b .LGpuRuntimeDeviceAcquire9e
.LGpuRuntimeDeviceAcquire9c:
	movs r0, #0x5e
.LGpuRuntimeDeviceAcquire9e:
	ldr r2, [sp, #0x14]
	ldr.w r3, [r8]
	cmp r2, r3
	beq .LGpuRuntimeDeviceAcquireac
	bl __stack_chk_fail
.LGpuRuntimeDeviceAcquireac:
	add sp, #0x1c
	pop.w {r4, r5, r6, r7, r8, sb, pc}
	nop 
.size GpuRuntimeDeviceAcquire, .-GpuRuntimeDeviceAcquire
.align 2
GpuRuntimeDeviceAcquireLiteralb4:
	.word __stack_chk_guard
GpuRuntimeDeviceAcquireLiteralb8:
	.word gpuRuntimeThreadName
GpuRuntimeDeviceAcquireLiteralbc:
	.word GpuRuntimeWorker
GpuRuntimeDeviceAcquireLiteralc0:
	.word gpuRuntimeLisrName
GpuRuntimeDeviceAcquireLiteralc4:
	.word GpuRuntimeInterrupt
GpuRuntimeDeviceAcquireLiteralc8:
	.word gpuRuntimeIsrName
#endif
.align 2
.global GpuRuntimeDeviceRelease
.type GpuRuntimeDeviceRelease, %function
.thumb_func
GpuRuntimeDeviceRelease:
	push {r3, r4, r5, lr}
	ldr r4, [r0, #0x38]
	ldr r3, [r4, #4]
	cbz r3, .LGpuRuntimeDeviceRelease30
	movs r5, #0
	ldr r0, [r4, #0xc]
	bl ksceKernelReleaseIntrHandler
	str r5, [r4, #0x1c]
	movs r1, #1
	ldr r0, [r4, #0x18]
	bl ksceKernelSetEventFlag
	mov r1, r5
	mov r2, r5
	ldr r0, [r4, #0x14]
	bl ksceKernelWaitThreadEnd
	ldr r0, [r4, #0x18]
	bl ksceKernelDeleteEventFlag
	str r5, [r4, #4]
	mov r0, r5
	pop {r3, r4, r5, pc}
.LGpuRuntimeDeviceRelease30:
	movs r0, #0x61
	pop {r3, r4, r5, pc}
.size GpuRuntimeDeviceRelease, .-GpuRuntimeDeviceRelease
.align 2
.align 2
.global GpuRuntimeStateSet
.type GpuRuntimeStateSet, %function
.thumb_func
GpuRuntimeStateSet:
	ldr r3, [r0, #0x38]
	ldr r2, [r3, #8]
	cbnz r2, .LGpuRuntimeStateSet14
	ldr r1, GpuRuntimeStateSetLiteral18
	strd r1, r0, [r3, #0x20]
	movs r1, #1
	str r1, [r3, #8]
	mov r0, r2
	bx lr
.LGpuRuntimeStateSet14:
	movs r0, #0x60
	bx lr
.size GpuRuntimeStateSet, .-GpuRuntimeStateSet
.align 2
GpuRuntimeStateSetLiteral18:
	.word GpuRuntimePendingCallback
.align 2
.global GpuRuntimeStateClear
.type GpuRuntimeStateClear, %function
.thumb_func
GpuRuntimeStateClear:
	ldr r3, [r0, #0x38]
	ldr r2, [r3, #8]
	cbz r2, .LGpuRuntimeStateClear10
	movs r0, #0
	strd r0, r0, [r3, #0x20]
	str r0, [r3, #8]
	bx lr
.LGpuRuntimeStateClear10:
	movs r0, #0x61
	bx lr
.size GpuRuntimeStateClear, .-GpuRuntimeStateClear
.align 2
.align 2
.global GpuRuntimeStateSignal
.type GpuRuntimeStateSignal, %function
.thumb_func
GpuRuntimeStateSignal:
	push {r3, lr}
	ldr r3, [r0, #0x38]
	ldr r2, [r3, #8]
	cbz r2, .LGpuRuntimeStateSignal10
	ldr r0, [r3, #0x18]
	movs r1, #1
	bl ksceKernelSetEventFlag
.LGpuRuntimeStateSignal10:
	movs r0, #0
	pop {r3, pc}
.size GpuRuntimeStateSignal, .-GpuRuntimeStateSignal
.align 2
#if FW_DEBUG
.align 2
.global GpuRuntimeWorker
.type GpuRuntimeWorker, %function
.thumb_func
GpuRuntimeWorker:
	ldr r3, GpuRuntimeWorkerLiteral60
	push {r4, r5, r6, r7, lr}
	sub sp, #0x14
	ldr r4, [r1]
	mov r7, r3
	ldr r2, [r3]
	str r2, [sp, #0xc]
.LGpuRuntimeWorkere:
	ldr r5, [r4, #0x1c]
	cmp r5, #1
	bne .LGpuRuntimeWorker46
	movs r6, #0
	add r3, sp, #8
	str r6, [sp]
	mov r1, r5
	ldr r0, [r4, #0x18]
	movs r2, #5
	bl ksceKernelWaitEventFlag
	ldr r3, [r4, #0x1c]
	cbz r3, .LGpuRuntimeWorker46
	cbnz r0, .LGpuRuntimeWorker32
	ldrd r2, r0, [r4, #0x20]
	blx r2
	b .LGpuRuntimeWorkere
.LGpuRuntimeWorker32:
	ldr r3, GpuRuntimeWorkerLiteral64
	mov r1, r6
	ldr r2, GpuRuntimeWorkerLiteral68
	strd r3, r0, [sp]
	mov r0, r5
	ldr r3, GpuRuntimeWorkerLiteral6c
	bl ksceKernelPrintfLevelWithInfo
	b .LGpuRuntimeWorkere
.LGpuRuntimeWorker46:
	movs r0, #0
	bl sceKernelExitDeleteThread
	ldr r2, [sp, #0xc]
	movs r0, #0
	ldr r3, [r7]
	cmp r2, r3
	beq .LGpuRuntimeWorker5a
	bl __stack_chk_fail
.LGpuRuntimeWorker5a:
	add sp, #0x14
	pop {r4, r5, r6, r7, pc}
	nop 
.size GpuRuntimeWorker, .-GpuRuntimeWorker
.align 2
GpuRuntimeWorkerLiteral60:
	.word __stack_chk_guard
GpuRuntimeWorkerLiteral64:
	.word gpuRuntimeWorkerFunction
GpuRuntimeWorkerLiteral68:
	.word gpuRuntimeWorkerInfo
GpuRuntimeWorkerLiteral6c:
	.word gpuRuntimeWorkerWaitFormat
#else
.align 2
.global GpuRuntimeWorker
.type GpuRuntimeWorker, %function
.thumb_func
GpuRuntimeWorker:
	ldr r3, GpuRuntimeWorkerLiteral4c
	push {r4, r5, lr}
	sub sp, #0x14
	ldr r4, [r1]
	mov r5, r3
	ldr r2, [r3]
	str r2, [sp, #0xc]
.LGpuRuntimeWorkere:
	ldr r1, [r4, #0x1c]
	cmp r1, #1
	bne .LGpuRuntimeWorker32
	movs r3, #0
	movs r2, #5
	str r3, [sp]
	add r3, sp, #8
	ldr r0, [r4, #0x18]
	bl ksceKernelWaitEventFlag
	ldr r3, [r4, #0x1c]
	cbz r3, .LGpuRuntimeWorker32
	cmp r0, #0
	bne .LGpuRuntimeWorkere
	ldrd r2, r0, [r4, #0x20]
	blx r2
	b .LGpuRuntimeWorkere
.LGpuRuntimeWorker32:
	movs r0, #0
	bl sceKernelExitDeleteThread
	ldr r2, [sp, #0xc]
	movs r0, #0
	ldr r3, [r5]
	cmp r2, r3
	beq .LGpuRuntimeWorker46
	bl __stack_chk_fail
.LGpuRuntimeWorker46:
	add sp, #0x14
	pop {r4, r5, pc}
	nop 
.size GpuRuntimeWorker, .-GpuRuntimeWorker
.align 2
GpuRuntimeWorkerLiteral4c:
	.word __stack_chk_guard
#endif
.align 2
.global GpuRuntimeControlCallback
.type GpuRuntimeControlCallback, %function
.thumb_func
GpuRuntimeControlCallback:
	push {r3, lr}
	ldr r3, [r1, #0xc]
	cbz r3, .LGpuRuntimeControlCallbackc
	ldrd r3, r0, [r1]
	blx r3
.LGpuRuntimeControlCallbackc:
	mov.w r0, #-1
	pop {r3, pc}
.size GpuRuntimeControlCallback, .-GpuRuntimeControlCallback
.align 2
.align 2
.global GpuRuntimeInterrupt
.type GpuRuntimeInterrupt, %function
.thumb_func
GpuRuntimeInterrupt:
	push {r4, lr}
	mov r4, r1
	cbz r1, .LGpuRuntimeInterrupt18
	mov r0, r1
	bl GpuRuntimeIrqProbe
	cbz r0, .LGpuRuntimeInterrupt18
	ldr r3, [r4, #0x38]
	movs r1, #1
	ldr r0, [r3, #0x18]
	bl ksceKernelSetEventFlag
.LGpuRuntimeInterrupt18:
	mov.w r0, #-1
	pop {r4, pc}
.size GpuRuntimeInterrupt, .-GpuRuntimeInterrupt
.align 2
.align 2
.global GpuRuntimeIrqProbe
.type GpuRuntimeIrqProbe, %function
.thumb_func
GpuRuntimeIrqProbe:
	ldr r3, GpuRuntimeIrqProbeLiteral48
	movs r1, #0
	push {r4, r5, lr}
	sub sp, #0x1c
	str r1, [sp, #8]
	mov r4, r0
	mov r5, r3
	ldr r2, [r3]
	str r1, [sp, #0x10]
	str r2, [sp, #0x14]
	cbz r0, .LGpuRuntimeIrqProbe36
	bl GpuRuntimeIrqMaskRead
	str r0, [sp, #0xc]
	cbz r0, .LGpuRuntimeIrqProbe36
	add r3, sp, #0x10
	ldr r1, GpuRuntimeIrqProbeLiteral4c
	str r3, [sp]
	add r2, sp, #8
	ldr r0, [r4, #0x1c]
	add r3, sp, #0xc
	bl GpuDeviceListForEachVariadic
	mov r0, r4
	ldr r1, [sp, #0x10]
	bl GpuRuntimeIrqMaskAck
.LGpuRuntimeIrqProbe36:
	ldr r2, [sp, #0x14]
	ldr r3, [r5]
	ldr r0, [sp, #8]
	cmp r2, r3
	beq .LGpuRuntimeIrqProbe44
	bl __stack_chk_fail
.LGpuRuntimeIrqProbe44:
	add sp, #0x1c
	pop {r4, r5, pc}
.size GpuRuntimeIrqProbe, .-GpuRuntimeIrqProbe
.align 2
GpuRuntimeIrqProbeLiteral48:
	.word __stack_chk_guard
GpuRuntimeIrqProbeLiteral4c:
	.word GpuRuntimeIrqVisitor
.align 2
.global GpuRuntimeIrqVisitor
.type GpuRuntimeIrqVisitor, %function
.thumb_func
GpuRuntimeIrqVisitor:
	push {r4, r5, r6, lr}
	mov r4, r0
	ldr r3, [r0, #0x28]
	ldrd r6, r2, [r1]
	ldr r5, [r1, #8]
	cbz r3, .LGpuRuntimeIrqVisitor28
	ldr r1, [r2]
	ldr r2, [r0, #0x30]
	tst r1, r2
	beq .LGpuRuntimeIrqVisitor28
	ldr r0, [r0, #0x2c]
	blx r3
	cbz r0, .LGpuRuntimeIrqVisitor20
	movs r3, #1
	str r3, [r6]
.LGpuRuntimeIrqVisitor20:
	ldr r3, [r5]
	ldr r2, [r4, #0x30]
	orrs r3, r2
	str r3, [r5]
.LGpuRuntimeIrqVisitor28:
	pop {r4, r5, r6, pc}
.size GpuRuntimeIrqVisitor, .-GpuRuntimeIrqVisitor
.align 2
.align 2
.global GpuRuntimeIrqMaskRead
.type GpuRuntimeIrqMaskRead, %function
.thumb_func
GpuRuntimeIrqMaskRead:
	mov.w r0, #-1
	bx lr
.size GpuRuntimeIrqMaskRead, .-GpuRuntimeIrqMaskRead
.align 2
.align 2
.global GpuRuntimeIrqMaskAck
.type GpuRuntimeIrqMaskAck, %function
.thumb_func
GpuRuntimeIrqMaskAck:
	bx lr
.size GpuRuntimeIrqMaskAck, .-GpuRuntimeIrqMaskAck
.align 2
.align 2
.global GpuRuntimePendingCallback
.type GpuRuntimePendingCallback, %function
.thumb_func
GpuRuntimePendingCallback:
	push {r3, lr}
	cbz r0, .LGpuRuntimePendingCallback14
	ldr r1, GpuRuntimePendingCallbackLiteral18
	ldr r0, [r0, #0x1c]
	bl GpuDeviceListForEach
	bl SignalWaitCounter
	bl GpuSignalListDispatch
.LGpuRuntimePendingCallback14:
	pop {r3, pc}
	nop 
.size GpuRuntimePendingCallback, .-GpuRuntimePendingCallback
.align 2
GpuRuntimePendingCallbackLiteral18:
	.word GpuRuntimePendingVisitor
.align 2
.global GpuRuntimePendingVisitor
.type GpuRuntimePendingVisitor, %function
.thumb_func
GpuRuntimePendingVisitor:
	push {r3, lr}
	ldr r3, [r0, #0x34]
	cbz r3, .LGpuRuntimePendingVisitora
	ldr r0, [r0, #0x2c]
	blx r3
.LGpuRuntimePendingVisitora:
	pop {r3, pc}
.size GpuRuntimePendingVisitor, .-GpuRuntimePendingVisitor
.align 2
.align 2
.global GpuSignalListLock
.type GpuSignalListLock, %function
.thumb_func
GpuSignalListLock:
	push {r3, lr}
	ldr r0, GpuSignalListLockLiteral10
	bl ksceKernelSpinlockLowLockCpuSuspendIntr
	ldr r3, GpuSignalListLockLiteral14
	str r0, [r3]
	pop {r3, pc}
	nop 
.size GpuSignalListLock, .-GpuSignalListLock
.align 2
GpuSignalListLockLiteral10:
	.word gpuSignalListLockWord
GpuSignalListLockLiteral14:
	.word gpuSignalListLockState
.align 2
.global GpuSignalListUnlock
.type GpuSignalListUnlock, %function
.thumb_func
GpuSignalListUnlock:
	push {r3, lr}
	ldr r3, GpuSignalListUnlockLiteral10
	ldr r0, GpuSignalListUnlockLiteral14
	ldr r1, [r3]
	bl ksceKernelSpinlockLowUnlockCpuResumeIntr
	pop {r3, pc}
	nop 
.size GpuSignalListUnlock, .-GpuSignalListUnlock
.align 2
GpuSignalListUnlockLiteral10:
	.word gpuSignalListLockState
GpuSignalListUnlockLiteral14:
	.word gpuSignalListLockWord
.align 2
.global GpuSignalListDispatch
.type GpuSignalListDispatch, %function
.thumb_func
GpuSignalListDispatch:
	push {r4, lr}
	bl GpuSignalListLock
	ldr r3, GpuSignalListDispatchLiteral2c
	ldr r4, [r3]
.LGpuSignalListDispatcha:
	cbz r4, .LGpuSignalListDispatch26
	ldr r3, [r4]
	cbz r3, .LGpuSignalListDispatch22
	ldr r0, [r4, #8]
	bl GpuSignalRecordValidate
	cbnz r0, .LGpuSignalListDispatch22
	str r0, [r4]
	ldrd r0, r1, [r4, #4]
	bl GpuSignalEventSet
.LGpuSignalListDispatch22:
	ldr r4, [r4, #0xc]
	b .LGpuSignalListDispatcha
.LGpuSignalListDispatch26:
	bl GpuSignalListUnlock
	pop {r4, pc}
.size GpuSignalListDispatch, .-GpuSignalListDispatch
.align 2
GpuSignalListDispatchLiteral2c:
	.word gpuSignalListHead
#if FW_DEBUG
.align 2
.global GpuSignalRecordValidate
.type GpuSignalRecordValidate, %function
.thumb_func
GpuSignalRecordValidate:
	push.w {r4, r5, r6, r7, r8, lr}
	mov r8, lr
	mov r5, r0
	cbz r0, .LGpuSignalRecordValidate5e
	ldr r3, [r0, #4]
	cmp r3, #0
	ble .LGpuSignalRecordValidate62
	movs r6, #0
	mov r4, r0
	mov r7, r6
.LGpuSignalRecordValidate16:
	ldr r3, [r5, #4]
	cmp r7, r3
	bge .LGpuSignalRecordValidate32
	ldr r0, [r4, #8]
	cbz r0, .LGpuSignalRecordValidate2c
	ldr r1, [r4, #0x1c]
	ldr r2, [r4, #0x2c]
	bl GpuSignalRecordRangeCheck
	cbnz r0, .LGpuSignalRecordValidate36
	adds r6, #1
.LGpuSignalRecordValidate2c:
	adds r7, #1
	adds r4, #4
	b .LGpuSignalRecordValidate16
.LGpuSignalRecordValidate32:
	movs r4, #7
	b .LGpuSignalRecordValidate38
.LGpuSignalRecordValidate36:
	movs r4, #6
.LGpuSignalRecordValidate38:
	ldr r3, [r5, #4]
	movt r4, #0x804c
	movs r0, #7
	ldr r1, GpuSignalRecordValidateLiteral6c
	movt r0, #0x804c
	mov r2, r8
	cmp r6, r3
	it eq
	moveq r4, #0
	subs r0, r4, r0
	it ne
	movne r0, #1
	bl ksceKernelAssert
	mov r0, r4
	pop.w {r4, r5, r6, r7, r8, pc}
.LGpuSignalRecordValidate5e:
	movs r0, #0
	b .LGpuSignalRecordValidate64
.LGpuSignalRecordValidate62:
	movs r0, #0xc
.LGpuSignalRecordValidate64:
	movt r0, #0x804c
	pop.w {r4, r5, r6, r7, r8, pc}
.size GpuSignalRecordValidate, .-GpuSignalRecordValidate
.align 2
GpuSignalRecordValidateLiteral6c:
	.word gpuSignalValidateInfo
#else
.align 2
.global GpuSignalRecordValidate
.type GpuSignalRecordValidate, %function
.thumb_func
GpuSignalRecordValidate:
	push {r3, r4, r5, r6, r7, lr}
	mov r4, r0
	cbz r0, .LGpuSignalRecordValidate44
	ldr r3, [r0, #4]
	cmp r3, #0
	ble .LGpuSignalRecordValidate48
	movs r6, #0
	mov r5, r0
	mov r7, r6
.LGpuSignalRecordValidate12:
	ldr r3, [r4, #4]
	cmp r7, r3
	bge .LGpuSignalRecordValidate2e
	ldr r0, [r5, #8]
	cbz r0, .LGpuSignalRecordValidate28
	ldr r1, [r5, #0x1c]
	ldr r2, [r5, #0x2c]
	bl GpuSignalRecordRangeCheck
	cbnz r0, .LGpuSignalRecordValidate32
	adds r6, #1
.LGpuSignalRecordValidate28:
	adds r7, #1
	adds r5, #4
	b .LGpuSignalRecordValidate12
.LGpuSignalRecordValidate2e:
	movs r3, #7
	b .LGpuSignalRecordValidate34
.LGpuSignalRecordValidate32:
	movs r3, #6
.LGpuSignalRecordValidate34:
	ldr r0, [r4, #4]
	movt r3, #0x804c
	cmp r6, r0
	ite ne
	movne r0, r3
	moveq r0, #0
	pop {r3, r4, r5, r6, r7, pc}
.LGpuSignalRecordValidate44:
	movs r0, #0
	b .LGpuSignalRecordValidate4a
.LGpuSignalRecordValidate48:
	movs r0, #0xc
.LGpuSignalRecordValidate4a:
	movt r0, #0x804c
	pop {r3, r4, r5, r6, r7, pc}
.size GpuSignalRecordValidate, .-GpuSignalRecordValidate
.align 2
#endif
.align 2
.global GpuSignalEventSet
.type GpuSignalEventSet, %function
.thumb_func
GpuSignalEventSet:
	cmp r0, #0
	push {r3, lr}
	ble .LGpuSignalEventSet10
	cbz r1, .LGpuSignalEventSet10
	ldr r1, [r1, #0x3c]
	bl ksceKernelSetEventFlag
	pop {r3, pc}
.LGpuSignalEventSet10:
	movs r0, #0
	movt r0, #0x804c
	pop {r3, pc}
.size GpuSignalEventSet, .-GpuSignalEventSet
.align 2
.align 2
.global GpuSignalRecordRangeCheck
.type GpuSignalRecordRangeCheck, %function
.thumb_func
GpuSignalRecordRangeCheck:
	push {r4, r5, lr}
	cbz r0, .LGpuSignalRecordRangeCheck24
	ldr r3, [r0]
	ldr r5, [r3]
	ldr r0, [r3, #4]
	ldr r4, [r3, #8]
	subs r2, r5, r2
	subs r5, r5, r0
	cmp r2, r5
	blo .LGpuSignalRecordRangeCheck28
	ldr r0, [r3, #0xc]
	subs r1, r4, r1
	subs r4, r4, r0
	cmp r1, r4
	ite lo
	movlo r0, #0x18
	movhs r0, #0
	pop {r4, r5, pc}
.LGpuSignalRecordRangeCheck24:
	movs r0, #3
	pop {r4, r5, pc}
.LGpuSignalRecordRangeCheck28:
	movs r0, #0x18
	pop {r4, r5, pc}
.size GpuSignalRecordRangeCheck, .-GpuSignalRecordRangeCheck
.align 2
.section .rodata
.global gpuRuntimeThreadName
gpuRuntimeThreadName:
	.byte 83,99,101,71,112,117,77,105,115,114,0
.global gpuRuntimeInstallFunction
gpuRuntimeInstallFunction:
	.byte 79,83,73,110,115,116,97,108,108,83,121,115,116,101,109,76,73,83,82,0
.global gpuRuntimeThreadCreateFormat
gpuRuntimeThreadCreateFormat:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,99,114,101,97,116,101,32,116,104,114,101,97,100,58,32,37,48,56,120,10,0
.global gpuRuntimeLisrName
gpuRuntimeLisrName:
	.byte 83,99,101,71,112,117,76,105,115,114,0
.global gpuRuntimeIsrName
gpuRuntimeIsrName:
	.byte 83,99,101,71,112,117,73,115,114,0
.global gpuRuntimeWorkerFunction
gpuRuntimeWorkerFunction:
	.byte 77,73,83,82,84,97,115,107,108,101,116,84,104,114,101,97,100,0
.global gpuRuntimeWorkerWaitFormat
gpuRuntimeWorkerWaitFormat:
	.byte 37,115,58,32,70,97,105,108,101,100,32,119,97,105,116,105,110,103,32,37,48,56,120,10,0
.global gpuRuntimeOsfuncFile
gpuRuntimeOsfuncFile:
	.byte 101,110,118,47,112,108,97,121,115,116,97,116,105,111,110,47,111,115,102,117,110,99,46,99,0
.global gpuSignalBridgeFile
gpuSignalBridgeFile:
	.byte 98,114,105,100,103,101,100,47,98,114,105,100,103,101,100,95,112,118,114,95,98,114,105,100,103,101,46,99,0
.align 2
.global gpuRuntimeInstallInfo
gpuRuntimeInstallInfo:
	.word 0xd8108c50,0x19f66bbe,0xb05e1be4
	.word gpuRuntimeOsfuncFile
	.word 894
.align 2
.global gpuRuntimeWorkerInfo
gpuRuntimeWorkerInfo:
	.word 0xd8108c50,0xcbead34a,0x60889eb4
	.word gpuRuntimeOsfuncFile
	.word 817
.align 2
.global gpuSignalValidateInfo
gpuSignalValidateInfo:
	.word 0x1399193b,0xcf67515a,0xcddd55c2
	.word gpuSignalBridgeFile
	.word 5248
.bss
.align 2
.global gpuSignalListLockState
.type gpuSignalListLockState, %object
gpuSignalListLockState:
	.space 4
.size gpuSignalListLockState,4
.global gpuSignalListHead
.type gpuSignalListHead, %object
gpuSignalListHead:
	.space 4
.size gpuSignalListHead,4
.global gpuSignalListLockWord
.type gpuSignalListLockWord, %object
gpuSignalListLockWord:
	.space 4
.size gpuSignalListLockWord,4
