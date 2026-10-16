/* Recovered callback/search/removal control flow, explicit original frames. */
.syntax unified
.thumb
.text
.align 2
.global ResourceMatchEqual
.type ResourceMatchEqual, %function
.thumb_func
ResourceMatchEqual:
	push {r4, r5, lr}
	ldmia.w r1, {r3, r5}
	ldr r4, [r1, #8]
	lsls r2, r3, #31
	ldr r1, [r1, #12]
	bpl.n .LResourceMatchEqual14
	ldr r2, [r0, #12]
	cmp r2, r5
	bne.n .LResourceMatchEqual2c
.LResourceMatchEqual14:
	lsls r2, r3, #30
	bpl.n .LResourceMatchEqual1e
	ldr r2, [r0, #16]
	cmp r2, r4
	bne.n .LResourceMatchEqual2c
.LResourceMatchEqual1e:
	lsls r3, r3, #29
	bpl.n .LResourceMatchEqual2e
	ldr r3, [r0, #20]
	cmp r3, r1
	it ne
	movne r0, #0
	pop {r4, r5, pc}
.LResourceMatchEqual2c:
	movs r0, #0
.LResourceMatchEqual2e:
	pop {r4, r5, pc}
.size ResourceMatchEqual, .-ResourceMatchEqual
.align 2
.align 2
.global ResourceMatchEqualAlternate
.type ResourceMatchEqualAlternate, %function
.thumb_func
ResourceMatchEqualAlternate:
	push {r4, r5, lr}
	ldmia.w r1, {r3, r5}
	ldr r4, [r1, #8]
	lsls r2, r3, #31
	ldr r1, [r1, #12]
	bpl.n .LResourceMatchEqualAlternate14
	ldr r2, [r0, #12]
	cmp r2, r5
	bne.n .LResourceMatchEqualAlternate2c
.LResourceMatchEqualAlternate14:
	lsls r2, r3, #30
	bpl.n .LResourceMatchEqualAlternate1e
	ldr r2, [r0, #16]
	cmp r2, r4
	bne.n .LResourceMatchEqualAlternate2c
.LResourceMatchEqualAlternate1e:
	lsls r3, r3, #29
	bpl.n .LResourceMatchEqualAlternate2e
	ldr r3, [r0, #20]
	cmp r3, r1
	it ne
	movne r0, #0
	pop {r4, r5, pc}
.LResourceMatchEqualAlternate2c:
	movs r0, #0
.LResourceMatchEqualAlternate2e:
	pop {r4, r5, pc}
.size ResourceMatchEqualAlternate, .-ResourceMatchEqualAlternate
.align 2
.align 2
.global ResourceSearch
.type ResourceSearch, %function
.thumb_func
ResourceSearch:
	push {r1, r2, r3}
	mov r3, r0
	push {r0, r1, r2, r4, r5, r6, lr}
	movs r0, #0
	ldr r2, ResourceSearchLiteral3c
	ldr r5, [sp, #28]
	ldr r1, [r2, #0]
	mov r4, r2
	str r1, [sp, #4]
.LResourceSearch12:
	cbnz r3, .LResourceSearch20
.LResourceSearch14:
	ldr r2, [sp, #4]
	ldr r3, [r4, #0]
	cmp r2, r3
	beq.n .LResourceSearch32
	bl ResourceStackFailure
.LResourceSearch20:
	cmp r0, #0
	bne.n .LResourceSearch14
	ldr r6, [r3, #4]
	add r1, sp, #32
	mov r0, r3
	str r1, [sp, #0]
	blx r5
	mov r3, r6
	b.n .LResourceSearch12
.LResourceSearch32:
	add sp, #12
	ldmia.w sp!, {r4, r5, r6, lr}
	add sp, #12
	bx lr
.size ResourceSearch, .-ResourceSearch
.align 2
.global ResourceSearchLiteral3c
ResourceSearchLiteral3c:
	.word __stack_chk_guard
.align 2
.global ResourceRemove
.type ResourceRemove, %function
.thumb_func
ResourceRemove:
	push {r3, r4, r5, lr}
	mov r4, r0
	ldrd r3, r0, [r4]
	str r0, [r3, #0]
	ldr r3, [r4, #4]
	cbz r3, .LResourceRemove12
	ldr r0, [r4, #0]
	str r0, [r3, #0]
.LResourceRemove12:
	cbz r1, .LResourceRemove20
	add.w r0, r4, #16
	ldmia r0, {r0, r1, r3}
	blx r3
	mov r5, r0
	b.n .LResourceRemove22
.LResourceRemove20:
	mov r5, r1
.LResourceRemove22:
	movs r1, #28
	mov r2, r4
	movs r3, #0
	movs r0, #1
	bl GpuFreeFlags
	mov r0, r5
	pop {r3, r4, r5, pc}
.size ResourceRemove, .-ResourceRemove
.align 2
.align 2
.global ResourceDrainTyped
.type ResourceDrainTyped, %function
.thumb_func
ResourceDrainTyped:
	stmdb sp!, {r0, r1, r4, r5, r6, r7, r8, lr}
	mov r6, r0
	mov r7, r1
	mov r8, r2
	mov r5, r3
	movs r4, #0
.LResourceDrainTypede:
	ldr r3, [sp, #32]
	mov r2, r7
	str r5, [sp, #0]
	ldr r0, [r6, #0]
	str r3, [sp, #4]
	mov r3, r8
	ldr r1, ResourceDrainTypedLiteral38
	bl ResourceSearch
	cbz r0, .LResourceDrainTyped30
	cbnz r4, .LResourceDrainTyped30
	mov r2, r4
	movs r1, #1
	bl ResourceRemove
	mov r4, r0
	b.n .LResourceDrainTypede
.LResourceDrainTyped30:
	mov r0, r4
	add sp, #8
	ldmia.w sp!, {r4, r5, r6, r7, r8, pc}
.size ResourceDrainTyped, .-ResourceDrainTyped
.align 2
.global ResourceDrainTypedLiteral38
ResourceDrainTypedLiteral38:
	.word ResourceMatchEqual
/* Legacy private map name was misleading: both predicates test equality. */
.global ResourceMatchNotEqual
.thumb_set ResourceMatchNotEqual, ResourceMatchEqualAlternate
.global ResourceStackFailure
.type ResourceStackFailure, %function
.thumb_func
ResourceStackFailure:
	ldr r12, =resourceServices
	ldr r12, [r12, #4]
	bx r12
