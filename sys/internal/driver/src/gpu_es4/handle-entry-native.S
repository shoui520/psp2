/* PLACEHOLDER: module_start service/literal binding remains pending.
 * Scoped behavioural gates supply genuine dependencies and qualified imports.
 */
/* Named native entry/ring/batch control flow. All instructions are symbolic;
 * no executable byte blobs. Frames and key locations follow the original ABI.
 * Actual bodies were compared across all four sanctioned originals first.
 */
.syntax unified
.thumb
.text

/* Remove the native key, detach sibling links, recursively free children, then publish/defer the free count. */
.global HandleEntryFree
.type HandleEntryFree, %function
.thumb_func
HandleEntryFree:
	push.w {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, lr}
	mov r4, r1
	ldr.w r1, handleGuardPointer
	mov r5, r0
	ldm.w r4, {r0, r6}
	ldr r3, [r1]
	mov r8, r1
	ldr r7, [r4, #0x14]
	str r3, [sp, #0xc]
	ldr r3, [r4, #0x10]
	ands r2, r3, #4
	it ne
	ldrne r2, [r4, #0x2c]
	strd r6, r0, [sp]
	str r2, [sp, #8]
	lsls r2, r3, #0x1e
	bmi .LHandleEntryFreeDetachParent
	ldr r3, [r4, #0xc]
	lsls r3, r3, #0x1e
	bmi .LHandleEntryFreeDetachParent
	ldr r0, [r5, #0xc]
	mov r1, sp
	bl HashRemoveExtended
.LHandleEntryFreeDetachParent:
	ldr r2, [r4, #0x14]
	ldr r3, [r4, #0x28]
	cmp r3, r2
	beq .LHandleEntryFreeLoadChildren
	ldr r1, [r4, #0x24]
	mov.w ip, #0xc
	ldr.w lr, [r5, #8]
	movs r6, #0x30
	lsrs r0, r1, #8
	uxtb.w sl, r1
	mul r0, ip, r0
	ldr.w sb, [lr, r0]
	ldr r0, [r4, #0x2c]
	subs r0, #1
	cmp r0, r1
	ite ne
	movne r1, #0x24
	moveq r1, #0x18
	cmp r0, r3
	ite ne
	movne r0, #0x24
	moveq r0, #0x18
	mla r1, r6, sl, r1
	add sb, r1
	lsrs r1, r3, #8
	mul ip, ip, r1
	ldr.w r1, [lr, ip]
	uxtb.w lr, r3
	str.w r3, [sb, #4]
	ldr r3, [r4, #0x24]
	mla r6, r6, lr, r1
	str r3, [r6, r0]
	movs r3, #0
	strd r2, r2, [r4, #0x24]
	str r3, [r4, #0x2c]
.LHandleEntryFreeLoadChildren:
	ldrd r6, r3, [r4, #0x1c]
	add.w sb, r3, #-1
.LHandleEntryFreeNextChild:
	cmp r6, sb
	beq .LHandleEntryFreeClearType
	lsrs r2, r6, #8
	movs r1, #0xc
	ldr r3, [r5, #8]
	uxtb r6, r6
	muls r2, r1, r2
	movs r1, #0x30
	mov r0, r5
	ldr r3, [r3, r2]
	mla r1, r1, r6, r3
	ldr r6, [r1, #0x28]
	bl HandleEntryFree
	cmp r0, #0
	beq .LHandleEntryFreeNextChild
	b .LHandleEntryFreeCheckGuard
.LHandleEntryFreeClearType:
	ldr r3, [r4, #0xc]
	movs r0, #0
	str r0, [r4]
	and r2, r3, #3
	cmp r2, #1
	bne .LHandleEntryFreePublishFreeEntry
	orr r3, r3, #2
	str r3, [r4, #0xc]
.LHandleEntryFreeCheckGuard:
	ldr r2, [sp, #0xc]
	ldr.w r3, [r8]
	cmp r2, r3
	beq .LHandleEntryFreeReturn
	bl HandleStackFailure
.LHandleEntryFreePublishFreeEntry:
	ldr r3, [r5, #0x34]
	cbnz r3, .LHandleEntryFreeIncrementFreeCounts
	ldr r2, [r5, #0x10]
	adds r3, r7, #1
	cbnz r2, .LHandleEntryFreeAppendFreeList
	str r7, [r5, #0x14]
	b .LHandleEntryFreeSetFreeTail
.LHandleEntryFreeAppendFreeList:
	ldr r2, [r5, #0x20]
	movs r4, #0xc
	ldr r1, [r5, #8]
	subs r2, #1
	lsrs r0, r2, #8
	uxtb r2, r2
	muls r0, r4, r0
	ldr r1, [r1, r0]
	movs r0, #0x30
	mla r2, r0, r2, r1
	str r3, [r2, #8]
.LHandleEntryFreeSetFreeTail:
	str r3, [r5, #0x20]
.LHandleEntryFreeIncrementFreeCounts:
	ldr r3, [r5, #0x10]
	lsrs r7, r7, #8
	movs r2, #0xc
	movs r0, #0
	adds r3, #1
	str r3, [r5, #0x10]
	ldr r3, [r5, #8]
	mla r7, r2, r7, r3
	ldr r3, [r7, #8]
	adds r3, #1
	str r3, [r7, #8]
	b .LHandleEntryFreeCheckGuard
.LHandleEntryFreeReturn:
	add sp, #0x10
	pop.w {r4, r5, r6, r7, r8, sb, sl, pc}
.size HandleEntryFree, .-HandleEntryFree

/* Commit survives only with zero pending operations; walk the genuine batch list and clear native state. */
.global HandleBatchFinish
.type HandleBatchFinish, %function
.thumb_func
HandleBatchFinish:
	push {r4, r5, r6, lr}
	mov r5, r0
	ldrd r4, r3, [r0, #0x2c]
	cmp r3, #0
	ite eq
	moveq r6, r1
	movne r6, #0
.LHandleBatchFinishNextBatchEntry:
	cbz r4, .LHandleBatchFinishClearBatchState
	subs r4, #1
	movs r1, #0xc
	lsrs r3, r4, #8
	ldr r2, [r5, #8]
	uxtb r4, r4
	muls r3, r1, r3
	movs r1, #0x30
	ldr r3, [r2, r3]
	mla r1, r1, r4, r3
	movs r3, #0
	ldr r4, [r1, #8]
	str r3, [r1, #8]
	ldr r3, [r1, #0xc]
	cbz r6, .LHandleBatchFinishReleaseEntry
	lsls r0, r3, #0x1e
	bpl .LHandleBatchFinishCommitSurvivor
.LHandleBatchFinishReleaseEntry:
	lsls r2, r3, #0x1e
	mov r0, r5
	itt pl
	bicpl r3, r3, #1
	strpl r3, [r1, #0xc]
	bl HandleEntryFree
	b .LHandleBatchFinishNextBatchEntry
.LHandleBatchFinishCommitSurvivor:
	bic r3, r3, #1
	str r3, [r1, #0xc]
	b .LHandleBatchFinishNextBatchEntry
.LHandleBatchFinishClearBatchState:
	str r4, [r5, #0x24]
	mov r0, r4
	strd r4, r4, [r5, #0x28]
	str r4, [r5, #0x30]
	pop {r4, r5, r6, pc}
	movs r0, r0
.size HandleBatchFinish, .-HandleBatchFinish

/* Select a real page entry, insert its native stack key before consumption, initialize rings and publish output. */
.global HandleEntryAllocate
.type HandleEntryAllocate, %function
.thumb_func
HandleEntryAllocate:
	push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr}
	sub sp, #0x1c
	mov r8, r2
	mov sb, r3
	mov r4, r0
	mov fp, r1
	ldrd r7, r2, [sp, #0x40]
	ldr.w r3, handleGuardPointer
	str r2, [sp, #4]
	ldr r2, [r3]
	mov sl, r3
	str r2, [sp, #0x14]
	ldr r2, [r0, #0x10]
	cbz r2, .LHandleEntryAllocateExpandCapacity
.LHandleEntryAllocateSelectEntry:
	ldr r3, [r4, #0x34]
	ldr r6, [r4, #0x14]
	cbnz r3, .LHandleEntryAllocatePreparePurgeScan
	lsrs r2, r6, #8
	movs r1, #0xc
	ldr r3, [r4, #8]
	movs r5, #0x30
	muls r1, r2, r1
	uxtb r2, r6
	ldr r3, [r3, r1]
	mla r5, r5, r2, r3
	b .LHandleEntryAllocateCheckHashFlags
.LHandleEntryAllocateExpandCapacity:
	movs r1, #1
	bl HandleCapacityExpand
	cmp r0, #0
	bne .LHandleEntryAllocateCheckGuard
	b .LHandleEntryAllocateSelectEntry
.LHandleEntryAllocatePreparePurgeScan:
	ldr r0, [r4, #0x1c]
	bic r3, r6, #0xff
	mvn r6, #0x80000000
	movs r5, #0
	subs r6, #0xff
.LHandleEntryAllocateNextPurgePage:
	cmp r3, r0
	bhs .LHandleEntryAllocateClearPurgeHead
	ldr r1, [r4, #8]
	lsrs r2, r3, #8
	mov.w lr, #0xc
	mla r2, lr, r2, r1
	ldr r2, [r2, #8]
	cbz r2, .LHandleEntryAllocateAdvancePurgePage
	mov r6, r3
.LHandleEntryAllocateScanPurgeEntry:
	add.w r2, r3, #0x100
	cmp r6, r2
	bhs .LHandleEntryAllocateAdvancePurgePage
	lsrs r2, r6, #8
	movs r5, #0xc
	mov.w lr, #0x30
	muls r5, r2, r5
	uxtb r2, r6
	mul r2, lr, r2
	ldr.w lr, [r1, r5]
	add.w r5, lr, r2
	ldr.w r2, [lr, r2]
	cbnz r2, .LHandleEntryAllocateAdvancePurgeEntry
	ldr r2, [r5, #0xc]
	cbz r2, .LHandleEntryAllocateAdvancePurgePage
.LHandleEntryAllocateAdvancePurgeEntry:
	adds r6, #1
	b .LHandleEntryAllocateScanPurgeEntry
.LHandleEntryAllocateAdvancePurgePage:
	add.w r3, r3, #0x100
	b .LHandleEntryAllocateNextPurgePage
.LHandleEntryAllocateClearPurgeHead:
	movs r3, #0
	str r3, [r4, #0x14]
.LHandleEntryAllocateCheckHashFlags:
	lsls r2, r7, #0x1e
	add.w r3, r6, #1
	bpl .LHandleEntryAllocateInsertHash
.LHandleEntryAllocateConsumeEntry:
	ldr r2, [r4, #0x10]
	lsrs r6, r6, #8
	movs r1, #0xc
	subs r2, #1
	str r2, [r4, #0x10]
	ldr r2, [r4, #8]
	mla r6, r1, r6, r2
	ldr r2, [r6, #8]
	subs r2, #1
	str r2, [r6, #8]
	ldr r2, [r4, #0x34]
	cbz r2, .LHandleEntryAllocateUpdateFreeHead
	b .LHandleEntryAllocateInitializeEntry
.LHandleEntryAllocateInsertHash:
	ldr r2, [sp, #4]
	add r1, sp, #8
	ldr r0, [r4, #0xc]
	str r3, [sp]
	str r2, [sp, #0x10]
	mov r2, r3
	strd r8, sb, [sp, #8]
	bl HashInsertExtended
	ldr r3, [sp]
	cmp r0, #0
	bne .LHandleEntryAllocateConsumeEntry
	movs r0, #0x98
	b .LHandleEntryAllocateCheckGuard
.LHandleEntryAllocateUpdateFreeHead:
	ldr r2, [r4, #0x10]
	cbnz r2, .LHandleEntryAllocateReadFreeLink
	str r2, [r4, #0x20]
	b .LHandleEntryAllocateStoreFreeHead
.LHandleEntryAllocateReadFreeLink:
	ldr r2, [r5, #8]
	cbz r2, .LHandleEntryAllocateUseSequentialIndex
	subs r2, #1
	b .LHandleEntryAllocateStoreFreeHead
.LHandleEntryAllocateUseSequentialIndex:
	mov r2, r3
.LHandleEntryAllocateStoreFreeHead:
	str r2, [r4, #0x14]
.LHandleEntryAllocateInitializeEntry:
	ldr r2, [r5, #0x14]
	movs r1, #0
	strd sb, r8, [r5]
	strd r1, r7, [r5, #0xc]
	adds r0, r2, #1
	strd r2, r2, [r5, #0x18]
	str r0, [r5, #0x20]
	strd r2, r2, [r5, #0x24]
	str r1, [r5, #0x2c]
	ldr r2, [r4, #0x24]
	cbz r2, .LHandleEntryAllocateClearBatchLink
	ldr r2, [r4, #0x2c]
	str r2, [r5, #8]
	str r3, [r4, #0x2c]
	ldr r2, [r5, #0xc]
	orr r2, r2, #1
	str r2, [r5, #0xc]
	b .LHandleEntryAllocatePublishHandle
.LHandleEntryAllocateClearBatchLink:
	str r2, [r5, #8]
.LHandleEntryAllocatePublishHandle:
	movs r0, #0
	str.w r3, [fp]
.LHandleEntryAllocateCheckGuard:
	ldr r2, [sp, #0x14]
	ldr.w r3, [sl]
	cmp r2, r3
	beq .LHandleEntryAllocateReturn
	bl HandleStackFailure
.LHandleEntryAllocateReturn:
	add sp, #0x1c
	pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc}
	nop 
.size HandleEntryAllocate, .-HandleEntryAllocate

/* Clear output; reuse a qualified hash match or allocate from the genuine page chain. */
.global HandleAllocate
.type HandleAllocate, %function
.thumb_func
HandleAllocate:
	push.w {r4, r5, r6, r7, r8, sb, sl, lr}
	mov r8, r3
	ldr.w r3, handleGuardPointer
	mov sl, r2
	sub sp, #0x20
	mov r4, r0
	mov r6, r1
	ldr r5, [sp, #0x40]
	ldr r2, [r3]
	mov sb, r3
	str r2, [sp, #0x1c]
	movs r2, #0
	str r2, [r1]
	ldr r2, [r0, #0x24]
	cbz r2, .LHandleAllocateCheckHashFlags
	ldr r3, [r0, #0x30]
	adds r3, #1
	str r3, [r0, #0x30]
.LHandleAllocateCheckHashFlags:
	ands r3, r5, #2
	bne .LHandleAllocateCreateEntry
	ldr r0, [r4, #0xc]
	add r1, sp, #0x10
	strd sl, r8, [sp, #0x10]
	str r3, [sp, #0x18]
	bl HashRetrieveExtended
	mov r7, r0
	cbz r0, .LHandleAllocateCreateEntry
	mov r0, r4
	add r1, sp, #0xc
	mov r2, r7
	mov r3, r8
	bl HandleEntryLookup
	cbnz r0, .LHandleAllocateCheckGuard
	ldr r3, [sp, #0xc]
	ldr r3, [r3, #0x10]
	and r3, r3, #1
	tst r3, r5
	beq .LHandleAllocateDenyReuse
	str r7, [r6]
	b .LHandleAllocateCompletePending
.LHandleAllocateDenyReuse:
	movs r0, #0x99
	b .LHandleAllocateCheckGuard
.LHandleAllocateCreateEntry:
	movs r3, #0
	strd r5, r3, [sp]
	mov r0, r4
	mov r1, r6
	mov r2, sl
	mov r3, r8
	bl HandleEntryAllocate
.LHandleAllocateCompletePending:
	ldr r3, [r4, #0x24]
	cbz r3, .LHandleAllocateCheckGuard
	cbnz r0, .LHandleAllocateCheckGuard
	ldr r3, [r4, #0x30]
	subs r3, #1
	str r3, [r4, #0x30]
.LHandleAllocateCheckGuard:
	ldr r2, [sp, #0x1c]
	ldr.w r3, [sb]
	cmp r2, r3
	beq .LHandleAllocateReturn
	bl HandleStackFailure
.LHandleAllocateReturn:
	add sp, #0x20
	pop.w {r4, r5, r6, r7, r8, sb, sl, pc}
	nop 
.size HandleAllocate, .-HandleAllocate

/* Validate the real parent, qualify the hash key when requested, and attach to its sentinel child ring. */
.global HandleAllocateWithParent
.type HandleAllocateWithParent, %function
.thumb_func
HandleAllocateWithParent:
	push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr}
	mov sb, r3
	ldr.w r7, handleGuardPointer
	sub sp, #0x2c
	mov r4, r0
	mov r8, r1
	ldrd r6, r5, [sp, #0x50]
	mov sl, r2
	ldr r3, [r7]
	str r3, [sp, #0x24]
	movs r3, #0
	str r3, [r1]
	ldr r3, [r0, #0x24]
	cbz r3, .LHandleAllocateWithParentValidateParent
	ldr r3, [r0, #0x30]
	adds r3, #1
	str r3, [r0, #0x30]
.LHandleAllocateWithParentValidateParent:
	tst.w r6, #4
	mov r0, r4
	add r1, sp, #0xc
	mov r2, r5
	mov.w r3, #0
	ite ne
	movne fp, r5
	moveq.w fp, #0
	bl HandleEntryLookup
	cmp r0, #0
	bne .LHandleAllocateWithParentCheckGuard
	lsls r3, r6, #0x1e
	bmi .LHandleAllocateWithParentCreateChild
	ldr r0, [r4, #0xc]
	add r1, sp, #0x18
	strd sl, sb, [sp, #0x18]
	str.w fp, [sp, #0x20]
	bl HashRetrieveExtended
	mov r2, r0
	str r0, [sp, #0x10]
	cbz r0, .LHandleAllocateWithParentCreateChild
	mov r0, r4
	add r1, sp, #0x14
	mov r3, sb
	bl HandleEntryLookup
	cmp r0, #0
	bne .LHandleAllocateWithParentCheckGuard
	ldr r3, [sp, #0x14]
	ldr r3, [r3, #0x10]
	and r3, r3, #1
	tst r3, r6
	beq .LHandleAllocateWithParentDenyReuse
	ldr r3, [sp, #0x10]
	movs r6, #0xc
	ldr r1, [r4, #8]
	subs r2, r3, #1
	lsrs r0, r2, #8
	uxtb r2, r2
	muls r0, r6, r0
	ldr r1, [r1, r0]
	movs r0, #0x30
	mla r2, r0, r2, r1
	ldr r2, [r2, #0x2c]
	cmp r2, r5
	bne .LHandleAllocateWithParentDenyReuse
	str.w r3, [r8]
	b .LHandleAllocateWithParentCompletePending
.LHandleAllocateWithParentDenyReuse:
	movs r0, #0x99
	b .LHandleAllocateWithParentCheckGuard
.LHandleAllocateWithParentCreateChild:
	stm.w sp, {r6, fp}
	mov r0, r4
	add r1, sp, #0x10
	mov r2, sl
	mov r3, sb
	bl HandleEntryAllocate
	cmp r0, #0
	bne .LHandleAllocateWithParentCheckGuard
	subs r5, #1
	movs r6, #0xc
	lsrs r3, r5, #8
	ldr r0, [r4, #8]
	ldr.w lr, [sp, #0x10]
	uxtb r5, r5
	muls r3, r6, r3
	movs r1, #0x30
	ldr r3, [r0, r3]
	mla r5, r1, r5, r3
	add.w r3, lr, #-1
	lsrs r2, r3, #8
	uxtb r3, r3
	str r5, [sp, #0xc]
	muls r2, r6, r2
	ldr.w ip, [r5, #0x20]
	add.w sl, ip, #-1
	ldr r2, [r0, r2]
	mla r3, r1, r3, r2
	ldr r2, [r5, #0x18]
	lsr.w fp, r2, #8
	ldr.w sb, [r3, #0x14]
	mul r6, r6, fp
	uxtb.w fp, r2
	ldr r6, [r0, r6]
	cmp r2, sl
	ite ne
	movne r0, #0x24
	moveq r0, #0x18
	str r2, [r3, #0x24]
	str.w sb, [r5, #0x18]
	str.w sl, [r3, #0x28]
	mla r1, r1, fp, r6
	add r1, r0
	str.w sb, [r1, #4]
	str.w ip, [r3, #0x2c]
	str.w lr, [r8]
.LHandleAllocateWithParentCompletePending:
	ldr r0, [r4, #0x24]
	cbz r0, .LHandleAllocateWithParentCheckGuard
	ldr r3, [r4, #0x30]
	movs r0, #0
	subs r3, #1
	str r3, [r4, #0x30]
.LHandleAllocateWithParentCheckGuard:
	ldr r2, [sp, #0x24]
	ldr r3, [r7]
	cmp r2, r3
	beq .LHandleAllocateWithParentReturn
	bl HandleStackFailure
.LHandleAllocateWithParentReturn:
	add sp, #0x2c
	pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc}
	nop 
.size HandleAllocateWithParent, .-HandleAllocateWithParent

/* Query the exact (data,type,zero) stack key without changing the output on failure. */
.global HandleFindByData
.type HandleFindByData, %function
.thumb_func
HandleFindByData:
	push {r0, r1, r2, r3, r4, r5, r6, lr}
	mov r6, r1
	ldr.w r4, handleGuardPointer
	movs r5, #0
	ldr r0, [r0, #0xc]
	ldr r1, [r4]
	stm.w sp, {r2, r3}
	str r5, [sp, #8]
	str r1, [sp, #0xc]
	mov r1, sp
	bl HashRetrieveExtended
	cbz r0, .LHandleFindByDataNotFound
	str r0, [r6]
	mov r0, r5
	b .LHandleFindByDataCheckGuard
.LHandleFindByDataNotFound:
	movs r0, #0x9a
.LHandleFindByDataCheckGuard:
	ldr r2, [sp, #0xc]
	ldr r3, [r4]
	cmp r2, r3
	beq .LHandleFindByDataReturn
	bl HandleStackFailure
.LHandleFindByDataReturn:
	add sp, #0x10
	pop {r4, r5, r6, pc}
.size HandleFindByData, .-HandleFindByData

/* Type-aware four-word lookup; publish opaque entry data only on success. */
.global HandleGetData
.type HandleGetData, %function
.thumb_func
HandleGetData:
	push {r0, r1, r2, r4, r5, lr}
	mov r5, r1
	ldr.w r4, handleGuardPointer
	ldr r1, [r4]
	str r1, [sp, #4]
	mov r1, sp
	bl HandleEntryLookup
	cbnz r0, .LHandleGetDataCheckGuard
	ldr r3, [sp]
	ldr r3, [r3, #4]
	str r3, [r5]
.LHandleGetDataCheckGuard:
	ldr r2, [sp, #4]
	ldr r3, [r4]
	cmp r2, r3
	beq .LHandleGetDataReturn
	bl HandleStackFailure
.LHandleGetDataReturn:
	add sp, #0xc
	pop {r4, r5, pc}
.size HandleGetData, .-HandleGetData

/* Validate handle and type before actual recursive entry removal. */
.global HandleRelease
.type HandleRelease, %function
.thumb_func
HandleRelease:
	push {r0, r1, r4, r5, r6, lr}
	mov r3, r2
	ldr.w r4, handleGuardPointer
	mov r6, r1
	mov r1, sp
	mov r5, r0
	ldr r2, [r4]
	str r2, [sp, #4]
	mov r2, r6
	bl HandleEntryLookup
	cbnz r0, .LHandleReleaseCheckGuard
	mov r0, r5
	ldr r1, [sp]
	bl HandleEntryFree
.LHandleReleaseCheckGuard:
	ldr r2, [sp, #4]
	ldr r3, [r4]
	cmp r2, r3
	beq .LHandleReleaseReturn
	bl HandleStackFailure
.LHandleReleaseReturn:
	add sp, #8
	pop {r4, r5, r6, pc}
.size HandleRelease, .-HandleRelease

/* Reserve genuine free capacity before publishing the active count and total-count snapshot. */
.global HandleBatchBegin
.type HandleBatchBegin, %function
.thumb_func
HandleBatchBegin:
	push {r3, r4, r5, lr}
	mov r4, r0
	ldr r3, [r0, #0x24]
	mov r5, r1
	cbnz r3, .LHandleBatchBeginAlreadyActive
	cbz r1, .LHandleBatchBeginZeroCount
	ldr r1, [r0, #0x10]
	cmp r5, r1
	bhi .LHandleBatchBeginExpandFreeCapacity
.LHandleBatchBeginPublishBatch:
	ldr r3, [r4, #0x1c]
	movs r0, #0
	strd r5, r3, [r4, #0x24]
	pop {r3, r4, r5, pc}
.LHandleBatchBeginExpandFreeCapacity:
	subs r1, r5, r1
	bl HandleCapacityExpand
	cmp r0, #0
	beq .LHandleBatchBeginPublishBatch
	pop {r3, r4, r5, pc}
.LHandleBatchBeginAlreadyActive:
	movs r0, #0x9c
	pop {r3, r4, r5, pc}
.LHandleBatchBeginZeroCount:
	movs r0, #3
	pop {r3, r4, r5, pc}
.size HandleBatchBegin, .-HandleBatchBegin

/* Request commit through the real pending-operation policy. */
.global HandleBatchCommit
.type HandleBatchCommit, %function
.thumb_func
HandleBatchCommit:
	push {r3, lr}
	ldr r3, [r0, #0x24]
	cbz r3, .LHandleBatchCommitInactive
	movs r1, #1
	bl HandleBatchFinish
	pop {r3, pc}
.LHandleBatchCommitInactive:
	movs r0, #3
	pop {r3, pc}
.size HandleBatchCommit, .-HandleBatchCommit

/* Abort only an active batch; preserve native opaque inactive r0 behavior. */
.global HandleBatchAbort
.type HandleBatchAbort, %function
.thumb_func
HandleBatchAbort:
	push {r3, lr}
	ldr r3, [r0, #0x24]
	cbz r3, .LHandleBatchAbortReturnOpaqueRegister
	movs r1, #0
	bl HandleBatchFinish
.LHandleBatchAbortReturnOpaqueRegister:
	pop {r3, pc}
.size HandleBatchAbort, .-HandleBatchAbort

/* Qualified relocated Sysclib guard pointer supplied by the fixture. */
.balign 4
.global handleGuardPointer
handleGuardPointer:
.word __stack_chk_guard
.global HandleStackFailure
.type HandleStackFailure, %function
.thumb_func
HandleStackFailure:
 ldr r12, =handleServices
 ldr r12, [r12, #4]
 bx r12
.size HandleStackFailure, .-HandleStackFailure
