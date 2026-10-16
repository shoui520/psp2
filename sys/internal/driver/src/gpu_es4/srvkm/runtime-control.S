/* Native startup service dependencies. Private names remain descriptive. */
.syntax unified
.thumb
.text
.align 2
.global GpuRuntimeDrainRequests
.type GpuRuntimeDrainRequests, %function
.thumb_func
GpuRuntimeDrainRequests:
	push.w {r0, r1, r2, r4, r5, r6, r7, r8, sb, lr}
	mov r6, r0
	mov r7, r1
	mov r0, r2
	movs r1, #0
	mov r5, r2
	mov sb, r3
	bl GpuRuntimePrecheck
	mov r2, r0
	cbnz r0, .LGpuRuntimeDrainRequests54
	ldr.w r8, GpuRuntimeDrainRequestsLiteral64
	str r7, [sp]
	ldr r1, GpuRuntimeDrainRequestsLiteral5c
	ldr.w r3, [r8]
	ldr r0, [r3, #0x20]
	mov r3, r6
	bl GpuRequestListVisitUntilError
	mov r4, r0
	cbnz r0, .LGpuRuntimeDrainRequests4c
	ldr.w r3, [r8]
	mov r2, r4
	str r7, [sp]
	ldr r1, GpuRuntimeDrainRequestsLiteral60
	ldr r0, [r3, #0x20]
	mov r3, r6
	bl GpuRequestListVisitUntilError
	mov r4, r0
	cmp.w sb, #0
	beq .LGpuRuntimeDrainRequests4c
	cbz r0, .LGpuRuntimeDrainRequests52
.LGpuRuntimeDrainRequests4c:
	mov r0, r5
	bl GpuRuntimeCheckEnd
.LGpuRuntimeDrainRequests52:
	mov r0, r4
.LGpuRuntimeDrainRequests54:
	add sp, #0xc
	pop.w {r4, r5, r6, r7, r8, sb, pc}
	nop 
.size GpuRuntimeDrainRequests, .-GpuRuntimeDrainRequests
.align 2
GpuRuntimeDrainRequestsLiteral64:
	.word gpuDumpRegion + 0x3178
GpuRuntimeDrainRequestsLiteral5c:
	.word GpuRequestTransitionPrepare
GpuRuntimeDrainRequestsLiteral60:
	.word GpuRequestTransitionCommit
.align 2
.global GpuRuntimePrecheck
.type GpuRuntimePrecheck, %function
.thumb_func
GpuRuntimePrecheck:
	ldr r3, GpuRuntimePrecheckLiteral60
	push.w {r4, r5, r6, r7, r8, lr}
	mov r5, r0
	mov r7, r1
	ldr.w r8, [r3]
	bl GpuRuntimeCriticalEnter
	cbnz r0, .LGpuRuntimePrecheck5a
	ldr r6, GpuRuntimePrecheckLiteral64
.LGpuRuntimePrecheck16:
	add.w r0, r8, #0x24
	mov r1, r5
	bl GpuServicePairTryAcquire
	mov r4, r0
	cbz r0, .LGpuRuntimePrecheck3c
	adds r3, r5, #3
	beq .LGpuRuntimePrecheck34
	movs r0, #1
	bl GpuBusyWaitClock
	subs r6, #1
	bne .LGpuRuntimePrecheck16
	b .LGpuRuntimePrecheck36
.LGpuRuntimePrecheck34:
	movs r4, #0x18
.LGpuRuntimePrecheck36:
	bl GpuRuntimeCriticalLeave
	b .LGpuRuntimePrecheck58
.LGpuRuntimePrecheck3c:
	cbnz r7, .LGpuRuntimePrecheck58
	ldr.w r3, [r8, #0x2c]
	cmp r3, #1
	ble .LGpuRuntimePrecheck52
	mov r0, r5
	bl GpuRuntimeCheckEnd
	movs r0, #0x18
	pop.w {r4, r5, r6, r7, r8, pc}
.LGpuRuntimePrecheck52:
	mov r0, r7
	pop.w {r4, r5, r6, r7, r8, pc}
.LGpuRuntimePrecheck58:
	mov r0, r4
.LGpuRuntimePrecheck5a:
	pop.w {r4, r5, r6, r7, r8, pc}
	nop 
.size GpuRuntimePrecheck, .-GpuRuntimePrecheck
.align 2
GpuRuntimePrecheckLiteral60:
	.word gpuDumpRegion + 0x3178
GpuRuntimePrecheckLiteral64:
	.word 1000000
.align 2
.global GpuRuntimeCheckEnd
.type GpuRuntimeCheckEnd, %function
.thumb_func
GpuRuntimeCheckEnd:
	push {r3, lr}
	mov r1, r0
	ldr r3, GpuRuntimeCheckEndLiteral14
	ldr r0, [r3]
	adds r0, #0x24
	bl GpuServicePairRelease
	bl GpuRuntimeCriticalLeave
	pop {r3, pc}
.size GpuRuntimeCheckEnd, .-GpuRuntimeCheckEnd
.align 2
GpuRuntimeCheckEndLiteral14:
	.word gpuDumpRegion + 0x3178
.align 2
.global GpuServicePairTryAcquire
.type GpuServicePairTryAcquire, %function
.thumb_func
GpuServicePairTryAcquire:
	push {r3, r4, r5, lr}
	movs r2, #1
	mov r5, r1
	movs r1, #0
	mov r4, r0
	bl ksceKernelAtomicCompareAndSet32
	cbnz r0, .LGpuServicePairTryAcquire14
	str r5, [r4, #4]
	pop {r3, r4, r5, pc}
.LGpuServicePairTryAcquire14:
	movs r0, #0x6a
	pop {r3, r4, r5, pc}
.size GpuServicePairTryAcquire, .-GpuServicePairTryAcquire
.align 2
.align 2
.global GpuServicePairRelease
.type GpuServicePairRelease, %function
.thumb_func
GpuServicePairRelease:
	ldr r3, [r0]
	cbz r3, .LGpuServicePairRelease14
	ldr r3, [r0, #4]
	cmp r3, r1
	bne .LGpuServicePairRelease18
	movs r3, #0
	str r3, [r0, #4]
	str r3, [r0]
	mov r0, r3
	bx lr
.LGpuServicePairRelease14:
	movs r0, #0x6c
	bx lr
.LGpuServicePairRelease18:
	movs r0, #0x6b
	bx lr
.size GpuServicePairRelease, .-GpuServicePairRelease
.align 2
.align 2
.global GpuServicePairIsOwned
.type GpuServicePairIsOwned, %function
.thumb_func
GpuServicePairIsOwned:
	ldr r3, [r0]
	cmp r3, #1
	bne .LGpuServicePairIsOwned10
	ldr r0, [r0, #4]
	subs r3, r0, r1
	rsbs r0, r3, #0
	adcs r0, r3
	bx lr
.LGpuServicePairIsOwned10:
	movs r0, #0
	bx lr
.size GpuServicePairIsOwned, .-GpuServicePairIsOwned
.align 2
.align 2
.global GpuRuntimeCriticalEnter
.type GpuRuntimeCriticalEnter, %function
.thumb_func
GpuRuntimeCriticalEnter:
	movs r0, #0
	bx lr
.size GpuRuntimeCriticalEnter, .-GpuRuntimeCriticalEnter
.align 2
.align 2
.global GpuRuntimeCriticalLeave
.type GpuRuntimeCriticalLeave, %function
.thumb_func
GpuRuntimeCriticalLeave:
	bx lr
.size GpuRuntimeCriticalLeave, .-GpuRuntimeCriticalLeave
.align 2
.align 2
.global GpuBusyWaitClock
.type GpuBusyWaitClock, %function
.thumb_func
GpuBusyWaitClock:
	push {r3, r4, r5, lr}
	mov r4, r0
	bl ksceKernelGetSystemTimeLow
	mov r5, r0
.LGpuBusyWaitClocka:
	bl ksceKernelGetSystemTimeLow
	subs r0, r0, r5
	cmp r0, r4
	blo .LGpuBusyWaitClocka
	pop {r3, r4, r5, pc}
.size GpuBusyWaitClock, .-GpuBusyWaitClock
.align 2
.align 2
.global GpuRequestListVisitUntilError
.type GpuRequestListVisitUntilError, %function
.thumb_func
GpuRequestListVisitUntilError:
	push {r1, r2, r3}
	mov r3, r0
	push {r0, r1, r2, r4, r5, r6, lr}
	movs r0, #0
	ldr r2, GpuRequestListVisitUntilErrorLiteral3c
	ldr r5, [sp, #0x1c]
	ldr r1, [r2]
	mov r4, r2
	str r1, [sp, #4]
.LGpuRequestListVisitUntilError12:
	cbnz r3, .LGpuRequestListVisitUntilError20
.LGpuRequestListVisitUntilError14:
	ldr r2, [sp, #4]
	ldr r3, [r4]
	cmp r2, r3
	beq .LGpuRequestListVisitUntilError32
	bl __stack_chk_fail
.LGpuRequestListVisitUntilError20:
	cmp r0, #0
	bne .LGpuRequestListVisitUntilError14
	ldr r6, [r3, #0x20]
	add r1, sp, #0x20
	mov r0, r3
	str r1, [sp]
	blx r5
	mov r3, r6
	b .LGpuRequestListVisitUntilError12
.LGpuRequestListVisitUntilError32:
	add sp, #0xc
	pop.w {r4, r5, r6, lr}
	add sp, #0xc
	bx lr
.size GpuRequestListVisitUntilError, .-GpuRequestListVisitUntilError
.align 2
GpuRequestListVisitUntilErrorLiteral3c:
	.word __stack_chk_guard
.align 2
.global GpuRequestListInsert
.type GpuRequestListInsert, %function
.thumb_func
GpuRequestListInsert:
	str r0, [r1, #0x24]
	ldr r3, [r0]
	str r3, [r1, #0x20]
	str r1, [r0]
	ldr r3, [r1, #0x20]
	cbz r3, .LGpuRequestListInsert10
	adds r1, #0x20
	str r1, [r3, #0x24]
.LGpuRequestListInsert10:
	bx lr
.size GpuRequestListInsert, .-GpuRequestListInsert
.align 2
.align 2
.global GpuRequestListUnlink
.type GpuRequestListUnlink, %function
.thumb_func
GpuRequestListUnlink:
	ldrd r2, r3, [r0, #0x20]
	str r2, [r3]
	ldr r3, [r0, #0x20]
	cbz r3, .LGpuRequestListUnlinke
	ldr r2, [r0, #0x24]
	str r2, [r3, #0x24]
.LGpuRequestListUnlinke:
	bx lr
.size GpuRequestListUnlink, .-GpuRequestListUnlink
.align 2
.align 2
.global GpuRequestTransitionPrepare
.type GpuRequestTransitionPrepare, %function
.thumb_func
GpuRequestTransitionPrepare:
	push {r3, r4, r5, lr}
	mov r4, r0
	ldm.w r1, {r0, r2, r5}
	cbnz r0, .LGpuRequestTransitionPrepare10
	ldr r3, [r4, #0x14]
	cmp r2, r3
	bne .LGpuRequestTransitionPrepare3a
.LGpuRequestTransitionPrepare10:
	adds r3, r5, #1
	ldr r2, [r4, #0x1c]
	it eq
	ldreq r5, [r4, #0x18]
	cmp r2, r5
	beq .LGpuRequestTransitionPrepare38
	ldr r3, [r4]
	cbnz r3, .LGpuRequestTransitionPrepare2c
.LGpuRequestTransitionPrepare20:
	ldr r0, [r4, #0x14]
	mov r1, r5
	ldr r2, [r4, #0x1c]
	bl GpuClockTransitionBefore
	pop {r3, r4, r5, pc}
.LGpuRequestTransitionPrepare2c:
	ldr r0, [r4, #0x10]
	mov r1, r5
	blx r3
	cmp r0, #0
	beq .LGpuRequestTransitionPrepare20
	pop {r3, r4, r5, pc}
.LGpuRequestTransitionPrepare38:
	movs r0, #0
.LGpuRequestTransitionPrepare3a:
	pop {r3, r4, r5, pc}
.size GpuRequestTransitionPrepare, .-GpuRequestTransitionPrepare
.align 2
.align 2
.global GpuRequestTransitionCommit
.type GpuRequestTransitionCommit, %function
.thumb_func
GpuRequestTransitionCommit:
	push {r3, r4, r5, lr}
	mov r4, r0
	ldm.w r1, {r0, r2, r5}
	cbnz r0, .LGpuRequestTransitionCommit10
	ldr r3, [r4, #0x14]
	cmp r2, r3
	bne .LGpuRequestTransitionCommit3e
.LGpuRequestTransitionCommit10:
	adds r3, r5, #1
	ldr r2, [r4, #0x1c]
	it eq
	ldreq r5, [r4, #0x18]
	cmp r2, r5
	beq .LGpuRequestTransitionCommit3c
	ldr r0, [r4, #0x14]
	mov r1, r5
	bl GpuClockTransitionAfter
	cbnz r0, .LGpuRequestTransitionCommit3e
	ldr r3, [r4, #4]
	cbnz r3, .LGpuRequestTransitionCommit2e
.LGpuRequestTransitionCommit2a:
	str r5, [r4, #0x1c]
	b .LGpuRequestTransitionCommit3c
.LGpuRequestTransitionCommit2e:
	ldr r0, [r4, #0x10]
	mov r1, r5
	ldr r2, [r4, #0x1c]
	blx r3
	cmp r0, #0
	beq .LGpuRequestTransitionCommit2a
	pop {r3, r4, r5, pc}
.LGpuRequestTransitionCommit3c:
	movs r0, #0
.LGpuRequestTransitionCommit3e:
	pop {r3, r4, r5, pc}
.size GpuRequestTransitionCommit, .-GpuRequestTransitionCommit
.align 2
.align 2
.global GpuClockTransitionBefore
.type GpuClockTransitionBefore, %function
.thumb_func
GpuClockTransitionBefore:
	push {r3, lr}
	ldr r3, GpuClockTransitionBeforeLiteral1c
	ldr r3, [r3]
	cmp r0, r3
	bne .LGpuClockTransitionBefore16
	cmp r1, r2
	beq .LGpuClockTransitionBefore16
	cmp r1, #2
	bne .LGpuClockTransitionBefore16
	bl ScePervasiveForDriver_E8656ADF
.LGpuClockTransitionBefore16:
	movs r0, #0
	pop {r3, pc}
	nop 
.size GpuClockTransitionBefore, .-GpuClockTransitionBefore
.align 2
GpuClockTransitionBeforeLiteral1c:
	.word gpuDumpRegion + 0x3134
.align 2
.global GpuClockTransitionAfter
.type GpuClockTransitionAfter, %function
.thumb_func
GpuClockTransitionAfter:
	push {r3, lr}
	ldr r3, GpuClockTransitionAfterLiteral1c
	ldr r3, [r3]
	cmp r0, r3
	bne .LGpuClockTransitionAfter16
	cmp r1, r2
	beq .LGpuClockTransitionAfter16
	cmp r2, #2
	bne .LGpuClockTransitionAfter16
	bl ScePervasiveForDriver_84EAB5BC
.LGpuClockTransitionAfter16:
	movs r0, #0
	pop {r3, pc}
	nop 
.size GpuClockTransitionAfter, .-GpuClockTransitionAfter
.align 2
GpuClockTransitionAfterLiteral1c:
	.word gpuDumpRegion + 0x3134
.align 2
.global GpuUnregisterDevice
.type GpuUnregisterDevice, %function
.thumb_func
GpuUnregisterDevice:
	ldr r3, GpuUnregisterDeviceLiteral78
	push {r0, r1, r2, r4, r5, r6, r7, lr}
	mov r5, r0
	ldr r1, GpuUnregisterDeviceLiteral7c
	mov r2, r5
	ldr r7, [r3]
	movs r3, #1
	ldr r0, [r7, #0x1c]
	bl GpuDeviceListFindVariadic
	mov r4, r0
	cbz r0, .LGpuUnregisterDevice70
	movs r3, #0
	mov r0, r5
	movs r1, #2
	mov.w r2, #-1
	bl GpuRuntimeDrainRequests
	mov r3, r0
	cbnz r0, .LGpuUnregisterDevice72
	str r0, [sp]
	movs r1, #1
	ldr r0, [r4, #0x5c]
	movs r2, #0x10
	bl ResourceDrainTyped
	cbnz r0, .LGpuUnregisterDevice72
	ldr r3, [r4, #0x1c]
	cbnz r3, .LGpuUnregisterDevice66
.LGpuUnregisterDevice3c:
	movs r1, #1
	movs r6, #0
	ldr r0, [r4, #0x5c]
	bl ResourceContextDestroy
	mov r0, r4
	str r6, [r4, #0x5c]
	bl GpuDeviceListUnlink
	mov r1, r5
	mov r0, r7
	bl GpuDeviceIndexRelease
	movs r0, #2
	movs r1, #0x70
	mov r2, r4
	mov r3, r6
	bl GpuFreeFlags
	mov r0, r6
	b .LGpuUnregisterDevice72
.LGpuUnregisterDevice66:
	mov r0, r4
	blx r3
	cmp r0, #0
	beq .LGpuUnregisterDevice3c
	b .LGpuUnregisterDevice72
.LGpuUnregisterDevice70:
	movs r0, #0xa5
.LGpuUnregisterDevice72:
	add sp, #0xc
	pop {r4, r5, r6, r7, pc}
	nop 
.size GpuUnregisterDevice, .-GpuUnregisterDevice
.align 2
GpuUnregisterDeviceLiteral78:
	.word gpuDumpRegion + 0x3178
GpuUnregisterDeviceLiteral7c:
	.word GpuDeviceMatchIndexClass
