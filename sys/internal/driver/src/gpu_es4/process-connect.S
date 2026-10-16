/* PLACEHOLDER: module_start service/literal binding remains pending.
 * Scoped behavioural gates supply genuine dependencies and qualified imports.
 */
/* Native Connect control flow. Stack allocator word escapes into the driver
 * and nested handle constructor; keep the actual direct-call boundaries. */
.syntax unified
.thumb
.text
.align 2
.global PVRSRVPerProcessDataConnectDebug
.type PVRSRVPerProcessDataConnectDebug, %function
.thumb_func
PVRSRVPerProcessDataConnectDebug:
	stmdb sp!, {r4, r5, r6, r7, r8, lr}
	mov r5, r0
	ldr r6, PVRSRVPerProcessDataConnectStackGuard
	sub sp, #264
	ldr r7, PVRSRVPerProcessDataConnectHashCell
	ldr r3, [r6, #0]
	ldr r0, [r7, #0]
	str r3, [sp, #260]
	cmp r0, #0
	beq.w .LPVRSRVPerProcessDataConnectMissingHash
	mov r1, r5
	bl HashRetrieve
	mov r4, r0
	str r0, [sp, #16]
	cmp r0, #0
	bne.w .LPVRSRVPerProcessDataConnectAddReference
	mov.w r8, #232
	mov r1, r4
	mov r2, r8
	add r0, sp, #28
	bl ConnectMemset
	mov r0, r5
	add r1, sp, #28
	str.w r8, [sp, #28]
	bl ConnectGetProcessInfo
	cmp r0, #0
	bge.n .LPVRSRVPerProcessDataConnectQuerySysver
	ldr r3, PVRSRVPerProcessDataConnectDiagnosticFunctionName
	mov r1, r4
	str r0, [sp, #8]
	movs r0, #1
	ldr r2, PVRSRVPerProcessDataConnectProcessInfoDebugRecord
	stmia.w sp, {r3, r5}
	ldr r3, PVRSRVPerProcessDataConnectProcessInfoFormat
	b.n .LPVRSRVPerProcessDataConnectReportQueryError
.LPVRSRVPerProcessDataConnectQuerySysver:
	mov r0, r5
	add r1, sp, #24
	bl ConnectGetProcessSysver
	cmp r0, #0
	bge.n .LPVRSRVPerProcessDataConnectAllocateProcessData
	ldr r3, PVRSRVPerProcessDataConnectDiagnosticFunctionName
	mov r1, r4
	ldr r2, PVRSRVPerProcessDataConnectSysverDebugRecord
	str r0, [sp, #8]
	movs r0, #1
	stmia.w sp, {r3, r5}
	ldr r3, PVRSRVPerProcessDataConnectSysverFormat
.LPVRSRVPerProcessDataConnectReportQueryError:
	bl ConnectPrintf
	movs r0, #3
	b.n .LPVRSRVPerProcessDataConnectCheckGuard
.LPVRSRVPerProcessDataConnectAllocateProcessData:
	movs r0, #2
	mov.w r1, #540
	add r2, sp, #16
	add r3, sp, #20
	bl ConnectAlloc
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectReturnSavedStatus
	ldr r0, [sp, #16]
	mov r1, r4
	mov.w r2, #540
	bl ConnectMemset
	ldrd r2, r3, [sp, #16]
	mov r1, r5
	ldr r0, [r7, #0]
	str r3, [r2, #4]
	bl HashInsert
	cmp r0, #0
	beq.n .LPVRSRVPerProcessDataConnectHashInsertFailed
	ldr r3, [sp, #16]
	ldr r2, [sp, #72]
	str r5, [r3, #0]
	str.w r2, [r3, #532]
	ldr r0, [sp, #16]
	ldr r3, [sp, #24]
	str r4, [r0, #24]
	adds r0, #32
	str.w r3, [r0, #504]
	bl ProcessMemoryContextInit
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectDestroyFailedProcess
	ldr r2, [sp, #16]
	ldr r3, PVRSRVPerProcessDataConnectGlobalHandleCell
	str r0, [sp, #0]
	add.w r1, r2, #12
	ldr r0, [r3, #0]
	movs r3, #1
	bl HandleAllocate
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectDestroyFailedProcess
	ldr r0, [sp, #16]
	adds r0, #16
	bl HandleBaseCreate
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectDestroyFailedProcess
	ldr r3, [sp, #16]
	ldr r0, [r3, #16]
	bl ProcessHandleSetup
	mov r4, r0
	cbz r0, .LPVRSRVPerProcessDataConnectContinueAfterOptionalFailure0
	b.w .LPVRSRVPerProcessDataConnectDestroyFailedProcess
.LPVRSRVPerProcessDataConnectContinueAfterOptionalFailure0:
	ldr r0, [sp, #16]
	add.w r1, r0, #8
	bl ResourceContextCreate
	mov r4, r0
	cbz r0, .LPVRSRVPerProcessDataConnectContinueAfterOptionalFailure1
	b.w .LPVRSRVPerProcessDataConnectDestroyFailedProcess
.LPVRSRVPerProcessDataConnectContinueAfterOptionalFailure1:
	ldr r5, [sp, #16]
	movs r3, #5
	movt r3, #1
	ldr r2, [r5, #0]
	cmp r2, r3
	bne.n .LPVRSRVPerProcessDataConnectCreateProcessEvent
	mov.w r3, #4294967295
	strd r3, r3, [r5, #524]
	b.n .LPVRSRVPerProcessDataConnectAddReference
.LPVRSRVPerProcessDataConnectCreateProcessEvent:
	mov.w r1, #4096
	mov r2, r4
	mov r3, r4
	ldr r0, PVRSRVPerProcessDataConnectEventName
	bl ConnectCreateEvent
	ldr r4, [sp, #16]
	str.w r0, [r5, #528]
	ldr.w r1, [r4, #528]
	cmp r1, #0
	blt.n .LPVRSRVPerProcessDataConnectProcessHandleFailed
	ldr r0, [r4, #0]
	bl ConnectOpenProcess
	ldr r3, [sp, #16]
	str.w r0, [r4, #524]
	ldr.w r2, [r3, #524]
	cmp r2, #0
	bge.n .LPVRSRVPerProcessDataConnectAddReference
	ldr.w r0, [r3, #528]
	bl ConnectDeleteEvent
	b.n .LPVRSRVPerProcessDataConnectProcessHandleFailed
.LPVRSRVPerProcessDataConnectHashInsertFailed:
	movs r4, #159
	b.n .LPVRSRVPerProcessDataConnectDestroyFailedProcess
.LPVRSRVPerProcessDataConnectProcessHandleFailed:
	movs r4, #1
.LPVRSRVPerProcessDataConnectDestroyFailedProcess:
	ldr r0, [sp, #16]
	bl DestroyPerProcessData
.LPVRSRVPerProcessDataConnectReturnSavedStatus:
	mov r0, r4
	b.n .LPVRSRVPerProcessDataConnectCheckGuard
.LPVRSRVPerProcessDataConnectAddReference:
	ldr r3, [sp, #16]
	movs r0, #0
	ldr r2, [r3, #24]
	adds r2, #1
	str r2, [r3, #24]
	b.n .LPVRSRVPerProcessDataConnectCheckGuard
.LPVRSRVPerProcessDataConnectMissingHash:
	movs r0, #4
.LPVRSRVPerProcessDataConnectCheckGuard:
	ldr r2, [sp, #260]
	ldr r3, [r6, #0]
	cmp r2, r3
	beq.n .LPVRSRVPerProcessDataConnectReturn
	bl ConnectStackFail
.LPVRSRVPerProcessDataConnectReturn:
	add sp, #264
	ldmia.w sp!, {r4, r5, r6, r7, r8, pc}
.align 2
.global PVRSRVPerProcessDataConnectStackGuard
PVRSRVPerProcessDataConnectStackGuard:
	.word 0
.global PVRSRVPerProcessDataConnectHashCell
PVRSRVPerProcessDataConnectHashCell:
	.word 0
.global PVRSRVPerProcessDataConnectDiagnosticFunctionName
PVRSRVPerProcessDataConnectDiagnosticFunctionName:
	.word 0
.global PVRSRVPerProcessDataConnectProcessInfoDebugRecord
PVRSRVPerProcessDataConnectProcessInfoDebugRecord:
	.word 0
.global PVRSRVPerProcessDataConnectProcessInfoFormat
PVRSRVPerProcessDataConnectProcessInfoFormat:
	.word 0
.global PVRSRVPerProcessDataConnectSysverDebugRecord
PVRSRVPerProcessDataConnectSysverDebugRecord:
	.word 0
.global PVRSRVPerProcessDataConnectSysverFormat
PVRSRVPerProcessDataConnectSysverFormat:
	.word 0
.global PVRSRVPerProcessDataConnectGlobalHandleCell
PVRSRVPerProcessDataConnectGlobalHandleCell:
	.word globalHandleBase
.global PVRSRVPerProcessDataConnectEventName
PVRSRVPerProcessDataConnectEventName:
	.word 0
.global PVRSRVPerProcessDataConnectRelease
.type PVRSRVPerProcessDataConnectRelease, %function
.thumb_func
PVRSRVPerProcessDataConnectRelease:
	push {r4, r5, r6, r7, lr}
	mov r5, r0
	ldr r6, PVRSRVPerProcessDataConnectReleaseStackGuard
	sub sp, #260
	ldr r7, PVRSRVPerProcessDataConnectReleaseHashCell
	ldr r3, [r6, #0]
	ldr r0, [r7, #0]
	str r3, [sp, #252]
	cmp r0, #0
	beq.w .LPVRSRVPerProcessDataConnectReleaseMissingHash
	mov r1, r5
	bl HashRetrieve
	mov r1, r0
	str r0, [sp, #8]
	cmp r0, #0
	bne.w .LPVRSRVPerProcessDataConnectReleaseAddReference
	movs r4, #232
	add r0, sp, #20
	mov r2, r4
	bl ConnectMemset
	mov r0, r5
	add r1, sp, #20
	str r4, [sp, #20]
	bl ConnectGetProcessInfo
	cmp r0, #0
	blt.n .LPVRSRVPerProcessDataConnectReleaseQueryFailed
	mov r0, r5
	add r1, sp, #16
	bl ConnectGetProcessSysver
	cmp r0, #0
	blt.n .LPVRSRVPerProcessDataConnectReleaseQueryFailed
	movs r0, #2
	mov.w r1, #540
	add r2, sp, #8
	add r3, sp, #12
	bl ConnectAlloc
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectReleaseReturnSavedStatus
	ldr r0, [sp, #8]
	mov r1, r4
	mov.w r2, #540
	bl ConnectMemset
	ldrd r2, r3, [sp, #8]
	mov r1, r5
	ldr r0, [r7, #0]
	str r3, [r2, #4]
	bl HashInsert
	cmp r0, #0
	beq.n .LPVRSRVPerProcessDataConnectReleaseHashInsertFailed
	ldr r3, [sp, #8]
	ldr r2, [sp, #64]
	str r5, [r3, #0]
	str.w r2, [r3, #532]
	ldr r0, [sp, #8]
	ldr r3, [sp, #16]
	str r4, [r0, #24]
	adds r0, #32
	str.w r3, [r0, #504]
	bl ProcessMemoryContextInit
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess
	ldr r2, [sp, #8]
	ldr r3, PVRSRVPerProcessDataConnectReleaseGlobalHandleCell
	str r0, [sp, #0]
	add.w r1, r2, #12
	ldr r0, [r3, #0]
	movs r3, #1
	bl HandleAllocate
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess
	ldr r0, [sp, #8]
	adds r0, #16
	bl HandleBaseCreate
	mov r4, r0
	cmp r0, #0
	bne.n .LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess
	ldr r3, [sp, #8]
	ldr r0, [r3, #16]
	bl ProcessHandleSetup
	mov r4, r0
	cbz r0, .LPVRSRVPerProcessDataConnectReleaseContinueAfterOptionalFailure0
	b.w .LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess
.LPVRSRVPerProcessDataConnectReleaseContinueAfterOptionalFailure0:
	ldr r0, [sp, #8]
	add.w r1, r0, #8
	bl ResourceContextCreate
	mov r4, r0
	cbz r0, .LPVRSRVPerProcessDataConnectReleaseContinueAfterOptionalFailure1
	b.w .LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess
.LPVRSRVPerProcessDataConnectReleaseContinueAfterOptionalFailure1:
	ldr r5, [sp, #8]
	movs r3, #5
	movt r3, #1
	ldr r2, [r5, #0]
	cmp r2, r3
	bne.n .LPVRSRVPerProcessDataConnectReleaseCreateProcessEvent
	mov.w r3, #4294967295
	strd r3, r3, [r5, #524]
	b.n .LPVRSRVPerProcessDataConnectReleaseAddReference
.LPVRSRVPerProcessDataConnectReleaseCreateProcessEvent:
	mov.w r1, #4096
	mov r2, r4
	mov r3, r4
	ldr r0, PVRSRVPerProcessDataConnectReleaseEventName
	bl ConnectCreateEvent
	ldr r4, [sp, #8]
	str.w r0, [r5, #528]
	ldr.w r1, [r4, #528]
	cmp r1, #0
	blt.n .LPVRSRVPerProcessDataConnectReleaseProcessHandleFailed
	ldr r0, [r4, #0]
	bl ConnectOpenProcess
	ldr r3, [sp, #8]
	str.w r0, [r4, #524]
	ldr.w r2, [r3, #524]
	cmp r2, #0
	bge.n .LPVRSRVPerProcessDataConnectReleaseAddReference
	ldr.w r0, [r3, #528]
	bl ConnectDeleteEvent
	b.n .LPVRSRVPerProcessDataConnectReleaseProcessHandleFailed
.LPVRSRVPerProcessDataConnectReleaseQueryFailed:
	movs r0, #3
	b.n .LPVRSRVPerProcessDataConnectReleaseCheckGuard
.LPVRSRVPerProcessDataConnectReleaseHashInsertFailed:
	movs r4, #159
	b.n .LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess
.LPVRSRVPerProcessDataConnectReleaseProcessHandleFailed:
	movs r4, #1
.LPVRSRVPerProcessDataConnectReleaseDestroyFailedProcess:
	ldr r0, [sp, #8]
	bl DestroyPerProcessData
.LPVRSRVPerProcessDataConnectReleaseReturnSavedStatus:
	mov r0, r4
	b.n .LPVRSRVPerProcessDataConnectReleaseCheckGuard
.LPVRSRVPerProcessDataConnectReleaseAddReference:
	ldr r3, [sp, #8]
	movs r0, #0
	ldr r2, [r3, #24]
	adds r2, #1
	str r2, [r3, #24]
	b.n .LPVRSRVPerProcessDataConnectReleaseCheckGuard
.LPVRSRVPerProcessDataConnectReleaseMissingHash:
	movs r0, #4
.LPVRSRVPerProcessDataConnectReleaseCheckGuard:
	ldr r2, [sp, #252]
	ldr r3, [r6, #0]
	cmp r2, r3
	beq.n .LPVRSRVPerProcessDataConnectReleaseReturn
	bl ConnectStackFail
.LPVRSRVPerProcessDataConnectReleaseReturn:
	add sp, #260
	pop {r4, r5, r6, r7, pc}
.align 2
.global PVRSRVPerProcessDataConnectReleaseStackGuard
PVRSRVPerProcessDataConnectReleaseStackGuard:
	.word 0
.global PVRSRVPerProcessDataConnectReleaseHashCell
PVRSRVPerProcessDataConnectReleaseHashCell:
	.word 0
.global PVRSRVPerProcessDataConnectReleaseGlobalHandleCell
PVRSRVPerProcessDataConnectReleaseGlobalHandleCell:
	.word globalHandleBase
.global PVRSRVPerProcessDataConnectReleaseEventName
PVRSRVPerProcessDataConnectReleaseEventName:
	.word 0
.global ConnectMemset
.type ConnectMemset, %function
.thumb_func
ConnectMemset:
	ldr.w pc, connectExternal+0
.global ConnectGetProcessInfo
.type ConnectGetProcessInfo, %function
.thumb_func
ConnectGetProcessInfo:
	ldr.w pc, connectExternal+4
.global ConnectGetProcessSysver
.type ConnectGetProcessSysver, %function
.thumb_func
ConnectGetProcessSysver:
	ldr.w pc, connectExternal+8
.global ConnectPrintf
.type ConnectPrintf, %function
.thumb_func
ConnectPrintf:
	ldr.w pc, connectExternal+12
.global ConnectAlloc
.type ConnectAlloc, %function
.thumb_func
ConnectAlloc:
	ldr.w pc, connectExternal+16
.global ConnectCreateEvent
.type ConnectCreateEvent, %function
.thumb_func
ConnectCreateEvent:
	ldr.w pc, connectExternal+20
.global ConnectOpenProcess
.type ConnectOpenProcess, %function
.thumb_func
ConnectOpenProcess:
	ldr.w pc, connectExternal+24
.global ConnectDeleteEvent
.type ConnectDeleteEvent, %function
.thumb_func
ConnectDeleteEvent:
	ldr.w pc, connectExternal+28
.global ConnectStackFail
.type ConnectStackFail, %function
.thumb_func
ConnectStackFail:
	ldr.w pc, connectExternal+32
.align 2
.global connectExternal
connectExternal:
	.space 36
.global ProcessMemoryContextInit
.type ProcessMemoryContextInit, %function
.thumb_func
ProcessMemoryContextInit:
	movs r0, #0
	bx lr
.global ProcessHandleSetup
.type ProcessHandleSetup, %function
.thumb_func
ProcessHandleSetup:
	movs r0, #0
	bx lr

/* One source tree, firmware-selected native ABI frames. */
.global PVRSRVPerProcessDataConnect
#if FW_DEBUG
.thumb_set PVRSRVPerProcessDataConnect, PVRSRVPerProcessDataConnectDebug
#else
.thumb_set PVRSRVPerProcessDataConnect, PVRSRVPerProcessDataConnectRelease
#endif
