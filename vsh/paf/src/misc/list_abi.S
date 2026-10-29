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
