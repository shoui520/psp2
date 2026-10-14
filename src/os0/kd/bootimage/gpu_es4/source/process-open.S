/* PLACEHOLDER: module_start service/literal binding remains pending.
 * Scoped behavioural gates supply genuine dependencies and qualified imports.
 */
.syntax unified
.thumb
.text
.align 2
.align 2
.global PVRSRVOpenDebug
.type PVRSRVOpenDebug, %function
.thumb_func
PVRSRVOpenDebug:
	push {r4, r5, r6, lr}
	mrc p15, 0, r5, c13, c0, 3
	lsls r3, r5, #16
	mcr p15, 0, r3, c13, c0, 3
	ldr r0, PVRSRVOpenLiteral48
	bl GpuMutexLock
	bl OSGetCurrentProcessIDKMDebug
	movs r1, #0
	bl GpuConnectWrapper
	mov r6, r0
	cbnz r0, .LPVRSRVOpen30
	bl OpenExternal642e0af2
	mov r4, r0
	cbnz r0, .LPVRSRVOpen36
	movs r1, #111
	bl OpenExternalb104efe2
	b.n .LPVRSRVOpen38
.LPVRSRVOpen30:
	mov.w r4, #4294967295
	b.n .LPVRSRVOpen38
.LPVRSRVOpen36:
	mov r4, r6
.LPVRSRVOpen38:
	ldr r0, PVRSRVOpenLiteral48
	bl GpuMutexUnlock
	mcr p15, 0, r5, c13, c0, 3
	mov r0, r4
	pop {r4, r5, r6, pc}
	nop 
.align 2
.global PVRSRVOpenLiteral48
PVRSRVOpenLiteral48:
	.word 0
.align 2
.global PVRSRVReleaseDebug
.type PVRSRVReleaseDebug, %function
.thumb_func
PVRSRVReleaseDebug:
	push {r3, r4, r5, lr}
	mrc p15, 0, r4, c13, c0, 3
	lsls r3, r4, #16
	mcr p15, 0, r3, c13, c0, 3
	ldr r5, PVRSRVReleaseLiteral2c
	mov r0, r5
	bl GpuMutexLock
	bl OSGetCurrentProcessIDKMDebug
	bl GpuDisconnectWrapper
	mov r0, r5
	bl GpuMutexUnlock
	mcr p15, 0, r4, c13, c0, 3
	movs r0, #0
	pop {r3, r4, r5, pc}
	nop 
.align 2
.global PVRSRVReleaseLiteral2c
PVRSRVReleaseLiteral2c:
	.word 0
.align 2
.global OSGetCurrentProcessIDKMDebug
.type OSGetCurrentProcessIDKMDebug, %function
.thumb_func
OSGetCurrentProcessIDKMDebug:
	push {r3, lr}
	bl OpenExternal9dcb4b7a
	pop {r3, pc}
.align 2
.align 2
.global GpuMutexCreate
.type GpuMutexCreate, %function
.thumb_func
GpuMutexCreate:
	push {r4, r5, lr}
	sub sp, #20
	ldr r4, GpuMutexCreateLiteral38
	movs r2, #0
	mov r5, r0
	mov.w r1, #8192
	ldr r0, GpuMutexCreateLiteral3c
	ldr r3, [r4, #0]
	str r2, [sp, #8]
	str r3, [sp, #12]
	movs r3, #8
	str r3, [sp, #4]
	add r3, sp, #4
	bl OpenExternalfbaa026e
	bic.w r0, r0, r0, asr #31
	ldr r2, [sp, #12]
	str r0, [r5, #0]
	ldr r3, [r4, #0]
	cmp r2, r3
	beq.n .LGpuMutexCreate32
	bl OpenExternalb997493d
.LGpuMutexCreate32:
	add sp, #20
	pop {r4, r5, pc}
	nop 
.align 2
.global GpuMutexCreateLiteral38
GpuMutexCreateLiteral38:
	.word 0
.global GpuMutexCreateLiteral3c
GpuMutexCreateLiteral3c:
	.word 0
.align 2
.global GpuMutexLockForPid
.type GpuMutexLockForPid, %function
.thumb_func
GpuMutexLockForPid:
	push {r0, r1, r4, lr}
	movs r2, #0
	mov r4, r1
	ldr r0, [r0, #0]
	movs r1, #1
	bl OpenExternal16ac80c5
	cbz r0, .LGpuMutexLockForPid28
	ldr r2, GpuMutexLockForPidLiteral34
	movs r1, #0
	ldr r3, GpuMutexLockForPidLiteral38
	strd r2, r0, [sp]
	movs r0, #1
	ldr r2, GpuMutexLockForPidLiteral3c
	bl OpenExternald9703808
	mov.w r0, #4294967295
	b.n .LGpuMutexLockForPid2a
.LGpuMutexLockForPid28:
	mov r0, r4
.LGpuMutexLockForPid2a:
	bl GpuSetCurrentProcessId
	add sp, #8
	pop {r4, pc}
	nop 
.align 2
.global GpuMutexLockForPidLiteral34
GpuMutexLockForPidLiteral34:
	.word 0
.global GpuMutexLockForPidLiteral38
GpuMutexLockForPidLiteral38:
	.word 0
.global GpuMutexLockForPidLiteral3c
GpuMutexLockForPidLiteral3c:
	.word 0
.align 2
.global GpuMutexLock
.type GpuMutexLock, %function
.thumb_func
GpuMutexLock:
	push {r4, lr}
	mov r4, r0
	bl OpenExternal9dcb4b7a
	mov r1, r0
	mov r0, r4
	bl GpuMutexLockForPid
	pop {r4, pc}
.align 2
.align 2
.global GpuMutexUnlock
.type GpuMutexUnlock, %function
.thumb_func
GpuMutexUnlock:
	push {r4, lr}
	mov r4, r0
	mov.w r0, #4294967295
	bl GpuSetCurrentProcessId
	ldr r0, [r4, #0]
	movs r1, #1
	bl OpenExternal1e82e5d0
	pop {r4, pc}
.align 2
.align 2
.global GpuSetCurrentProcessId
.type GpuSetCurrentProcessId, %function
.thumb_func
GpuSetCurrentProcessId:
	ldr r3, GpuSetCurrentProcessIdLiteralc
	ldr r2, [r3, #0]
	str r0, [r3, #0]
	mov r0, r2
	bx lr
	nop 
.align 2
.global GpuSetCurrentProcessIdLiteralc
GpuSetCurrentProcessIdLiteralc:
	.word remoteAllocProcess
.align 2
.global GpuConnectWrapper
.type GpuConnectWrapper, %function
.thumb_func
GpuConnectWrapper:
	push {r3, lr}
	bl PVRSRVPerProcessDataConnectDebug
	pop {r3, pc}
.align 2
.align 2
.global GpuDisconnectWrapper
.type GpuDisconnectWrapper, %function
.thumb_func
GpuDisconnectWrapper:
	push {r3, lr}
	bl PVRSRVPerProcessDataDisconnect
	pop {r3, pc}
.align 2
.align 2
.global PVRSRVOpenRelease
.type PVRSRVOpenRelease, %function
.thumb_func
PVRSRVOpenRelease:
	push {r4, r5, r6, lr}
	mrc p15, 0, r5, c13, c0, 3
	lsls r3, r5, #16
	mcr p15, 0, r3, c13, c0, 3
	ldr r0, PVRSRVOpenReleaseLiteral48
	bl GpuMutexLockRelease
	bl OSGetCurrentProcessIDKMRelease
	movs r1, #0
	bl GpuConnectWrapperRelease
	mov r6, r0
	cbnz r0, .LPVRSRVOpenRelease30
	bl OpenExternal642e0af2
	mov r4, r0
	cbnz r0, .LPVRSRVOpenRelease36
	movs r1, #111
	bl OpenExternalb104efe2
	b.n .LPVRSRVOpenRelease38
.LPVRSRVOpenRelease30:
	mov.w r4, #4294967295
	b.n .LPVRSRVOpenRelease38
.LPVRSRVOpenRelease36:
	mov r4, r6
.LPVRSRVOpenRelease38:
	ldr r0, PVRSRVOpenReleaseLiteral48
	bl GpuMutexUnlockRelease
	mcr p15, 0, r5, c13, c0, 3
	mov r0, r4
	pop {r4, r5, r6, pc}
	nop 
.align 2
.global PVRSRVOpenReleaseLiteral48
PVRSRVOpenReleaseLiteral48:
	.word 0
.align 2
.global PVRSRVReleaseRelease
.type PVRSRVReleaseRelease, %function
.thumb_func
PVRSRVReleaseRelease:
	push {r3, r4, r5, lr}
	mrc p15, 0, r4, c13, c0, 3
	lsls r3, r4, #16
	mcr p15, 0, r3, c13, c0, 3
	ldr r5, PVRSRVReleaseReleaseLiteral2c
	mov r0, r5
	bl GpuMutexLockRelease
	bl OSGetCurrentProcessIDKMRelease
	bl GpuDisconnectWrapperRelease
	mov r0, r5
	bl GpuMutexUnlockRelease
	mcr p15, 0, r4, c13, c0, 3
	movs r0, #0
	pop {r3, r4, r5, pc}
	nop 
.align 2
.global PVRSRVReleaseReleaseLiteral2c
PVRSRVReleaseReleaseLiteral2c:
	.word 0
.align 2
.global OSGetCurrentProcessIDKMRelease
.type OSGetCurrentProcessIDKMRelease, %function
.thumb_func
OSGetCurrentProcessIDKMRelease:
	push {r3, lr}
	bl OpenExternal9dcb4b7a
	pop {r3, pc}
.align 2
.align 2
.global GpuMutexCreateRelease
.type GpuMutexCreateRelease, %function
.thumb_func
GpuMutexCreateRelease:
	push {r4, r5, lr}
	sub sp, #20
	ldr r4, GpuMutexCreateReleaseLiteral38
	movs r2, #0
	mov r5, r0
	mov.w r1, #8192
	ldr r0, GpuMutexCreateReleaseLiteral3c
	ldr r3, [r4, #0]
	str r2, [sp, #8]
	str r3, [sp, #12]
	movs r3, #8
	str r3, [sp, #4]
	add r3, sp, #4
	bl OpenExternalfbaa026e
	bic.w r0, r0, r0, asr #31
	ldr r2, [sp, #12]
	str r0, [r5, #0]
	ldr r3, [r4, #0]
	cmp r2, r3
	beq.n .LGpuMutexCreateRelease32
	bl OpenExternalb997493d
.LGpuMutexCreateRelease32:
	add sp, #20
	pop {r4, r5, pc}
	nop 
.align 2
.global GpuMutexCreateReleaseLiteral38
GpuMutexCreateReleaseLiteral38:
	.word 0
.global GpuMutexCreateReleaseLiteral3c
GpuMutexCreateReleaseLiteral3c:
	.word 0
.align 2
.global GpuMutexLockForPidRelease
.type GpuMutexLockForPidRelease, %function
.thumb_func
GpuMutexLockForPidRelease:
	push {r4, lr}
	movs r2, #0
	mov r4, r1
	ldr r0, [r0, #0]
	movs r1, #1
	bl OpenExternal16ac80c5
	cbz r0, .LGpuMutexLockForPidRelease16
	mov.w r0, #4294967295
	b.n .LGpuMutexLockForPidRelease18
.LGpuMutexLockForPidRelease16:
	mov r0, r4
.LGpuMutexLockForPidRelease18:
	bl GpuSetCurrentProcessIdRelease
	pop {r4, pc}
.align 2
.align 2
.global GpuMutexLockRelease
.type GpuMutexLockRelease, %function
.thumb_func
GpuMutexLockRelease:
	push {r4, lr}
	mov r4, r0
	bl OpenExternal9dcb4b7a
	mov r1, r0
	mov r0, r4
	bl GpuMutexLockForPidRelease
	pop {r4, pc}
.align 2
.align 2
.global GpuMutexUnlockRelease
.type GpuMutexUnlockRelease, %function
.thumb_func
GpuMutexUnlockRelease:
	push {r4, lr}
	mov r4, r0
	mov.w r0, #4294967295
	bl GpuSetCurrentProcessIdRelease
	ldr r0, [r4, #0]
	movs r1, #1
	bl OpenExternal1e82e5d0
	pop {r4, pc}
.align 2
.align 2
.global GpuSetCurrentProcessIdRelease
.type GpuSetCurrentProcessIdRelease, %function
.thumb_func
GpuSetCurrentProcessIdRelease:
	ldr r3, GpuSetCurrentProcessIdReleaseLiteralc
	ldr r2, [r3, #0]
	str r0, [r3, #0]
	mov r0, r2
	bx lr
	nop 
.align 2
.global GpuSetCurrentProcessIdReleaseLiteralc
GpuSetCurrentProcessIdReleaseLiteralc:
	.word remoteAllocProcess
.align 2
.global GpuConnectWrapperRelease
.type GpuConnectWrapperRelease, %function
.thumb_func
GpuConnectWrapperRelease:
	push {r3, lr}
	bl PVRSRVPerProcessDataConnectRelease
	pop {r3, pc}
.align 2
.align 2
.global GpuDisconnectWrapperRelease
.type GpuDisconnectWrapperRelease, %function
.thumb_func
GpuDisconnectWrapperRelease:
	push {r3, lr}
	bl PVRSRVPerProcessDataDisconnect
	pop {r3, pc}
.align 2
.global OpenExternal16ac80c5
.type OpenExternal16ac80c5, %function
.thumb_func
OpenExternal16ac80c5:
	ldr.w pc, openExternal+0
.global OpenExternal1e82e5d0
.type OpenExternal1e82e5d0, %function
.thumb_func
OpenExternal1e82e5d0:
	ldr.w pc, openExternal+4
.global OpenExternal642e0af2
.type OpenExternal642e0af2, %function
.thumb_func
OpenExternal642e0af2:
	ldr.w pc, openExternal+8
.global OpenExternal9dcb4b7a
.type OpenExternal9dcb4b7a, %function
.thumb_func
OpenExternal9dcb4b7a:
	ldr.w pc, openExternal+12
.global OpenExternalb104efe2
.type OpenExternalb104efe2, %function
.thumb_func
OpenExternalb104efe2:
	ldr.w pc, openExternal+16
.global OpenExternalb997493d
.type OpenExternalb997493d, %function
.thumb_func
OpenExternalb997493d:
	ldr.w pc, openExternal+20
.global OpenExternald9703808
.type OpenExternald9703808, %function
.thumb_func
OpenExternald9703808:
	ldr.w pc, openExternal+24
.global OpenExternalfbaa026e
.type OpenExternalfbaa026e, %function
.thumb_func
OpenExternalfbaa026e:
	ldr.w pc, openExternal+28
.align 2
.global openExternal
openExternal:
	.space 32

/* One source tree, firmware-selected native ABI frames. */
.global PVRSRVOpen
#if FW_DEBUG
.thumb_set PVRSRVOpen, PVRSRVOpenDebug
#else
.thumb_set PVRSRVOpen, PVRSRVOpenRelease
#endif
.global PVRSRVRelease
#if FW_DEBUG
.thumb_set PVRSRVRelease, PVRSRVReleaseDebug
#else
.thumb_set PVRSRVRelease, PVRSRVReleaseRelease
#endif
.global OSGetCurrentProcessIDKM
#if FW_DEBUG
.thumb_set OSGetCurrentProcessIDKM, OSGetCurrentProcessIDKMDebug
#else
.thumb_set OSGetCurrentProcessIDKM, OSGetCurrentProcessIDKMRelease
#endif
