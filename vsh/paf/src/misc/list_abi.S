/* Original PAF doubly-linked list node removal and allocator call. */
	.syntax unified
	.thumb
	.text

	.global ScePafMisc_0078AC09
	.type ScePafMisc_0078AC09, %function
	.thumb_func
ScePafMisc_0078AC09:
	.fnstart
	push {r4, lr}
	adds r4, r0, #0
	cbnz r1, .Lvalid007
	movw r0, #0x5002
	movt r0, #0x80af
	b .Lreturn007
.Lvalid007:
	ldr r2, [r4]
	movs r3, #0
	cbz r2, .Linitial007
	ldr r3, [r2, #4]
.Linitial007:
	adds r0, r3, #0
	cmp r0, r2
	beq .Lnotfound007
.Lloop007:
	ldr r3, [r0, #8]
	cmp r1, r3
	bne .Ladvance007
	ldr r2, [r0]
	ldr r3, [r0, #4]
	str r2, [r3]
	ldr r1, [r0, #4]
	str r1, [r2, #4]
	bl sce_paf_free
	ldr r0, [r4, #4]
	subs r0, #1
	str r0, [r4, #4]
	movs r0, #0
	b .Lreturn007
.Ladvance007:
	cbz r0, .Ltest007
	ldr r0, [r0, #4]
.Ltest007:
	cmp r0, r2
	bne .Lloop007
.Lnotfound007:
	movs r0, #0
.Lreturn007:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_0078AC09, . - ScePafMisc_0078AC09

	.global ScePafMisc_EB1C375C
	.type ScePafMisc_EB1C375C, %function
	.thumb_func
ScePafMisc_EB1C375C:
	.fnstart
	push {r4, lr}
	adds r4, r0, #0
	ldr r2, [r4, #8]
	cbnz r2, .LactiveEB1
	movs r0, #0
	b .LreturnEB1
.LactiveEB1:
	ldr r1, [r4, #4]
	ldr r0, [r1]
	ldr r0, [r0, #8]
	cbnz r0, .LreturnEB1
.LloopEB1:
	cbz r1, .LtestEB1
	ldr r0, [r1]
	ldr r2, [r0]
	ldr r3, [r0, #4]
	str r2, [r3]
	ldr r1, [r0, #4]
	str r1, [r2, #4]
	bl sce_paf_free
	ldr r2, [r4, #8]
	subs r2, #1
	ldr r1, [r4, #4]
	str r2, [r4, #8]
.LtestEB1:
	cmp r2, #0
	bne .LloopEB1
	movs r0, #0
.LreturnEB1:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_EB1C375C, . - ScePafMisc_EB1C375C

/* The two exported list destructors have identical native instruction bodies. */
	.macro PafDestroyList name
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, r5, r6, lr}
	adds r4, r0, #0
	ldr r0, [r4]
	cbz r0, .Lempty\@
	ldr r1, [r0, #4]
	cmp r1, r0
	beq .Lfreehead\@
.Lloop\@:
	ldr r5, [r1, #4]
	adds r0, r1, #0
	bl sce_paf_free
	ldr r0, [r4]
	adds r1, r5, #0
	mov r2, r0
	cmp r1, r2
	bne .Lloop\@
.Lfreehead\@:
	bl sce_paf_free
.Lempty\@:
	movs.w r1, #-1
	str r1, [r4]
	adds r0, r4, #0
	str r1, [r4, #4]
	pop {r4, r5, r6, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafDestroyList ScePafMisc_04B4652F
	PafDestroyList ScePafMisc_600A1245
