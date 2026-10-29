/* Native Thumb enum-to-element-size lookup with original 34-way TBH table. */
	.syntax unified
	.thumb
	.text
	.global ScePafMisc_B207334F
	.type ScePafMisc_B207334F, %function
	.thumb_func
ScePafMisc_B207334F:
	.fnstart
	adds r0, #1
	cmp r0, #0x21
	bhi .LdefaultB207
	tbh [pc, r0, lsl #1]
.LtableB207:
	.hword (.Lcase0B207 - .LtableB207) / 2
	.hword (.Lcase1B207 - .LtableB207) / 2
	.hword (.Lcase2B207 - .LtableB207) / 2
	.hword (.Lcase3B207 - .LtableB207) / 2
	.hword (.Lcase4B207 - .LtableB207) / 2
	.hword (.Lcase5B207 - .LtableB207) / 2
	.hword (.LdefaultB207 - .LtableB207) / 2
	.hword (.Lcase7B207 - .LtableB207) / 2
	.rept 7
	.hword (.LdefaultB207 - .LtableB207) / 2
	.endr
	.hword (.Lcase15B207 - .LtableB207) / 2
	.hword (.Lcase15B207 - .LtableB207) / 2
	.hword (.Lcase17B207 - .LtableB207) / 2
	.hword (.LdefaultB207 - .LtableB207) / 2
	.hword (.Lcase19B207 - .LtableB207) / 2
	.hword (.LdefaultB207 - .LtableB207) / 2
	.hword (.Lcase21B207 - .LtableB207) / 2
	.hword (.Lcase22B207 - .LtableB207) / 2
	.hword (.Lcase23B207 - .LtableB207) / 2
	.hword (.Lcase24B207 - .LtableB207) / 2
	.hword (.Lcase25B207 - .LtableB207) / 2
	.hword (.Lcase26B207 - .LtableB207) / 2
	.rept 3
	.hword (.LdefaultB207 - .LtableB207) / 2
	.endr
	.hword (.Lcase30B207 - .LtableB207) / 2
	.hword (.Lcase31B207 - .LtableB207) / 2
	.hword (.Lcase30B207 - .LtableB207) / 2
	.hword (.Lcase31B207 - .LtableB207) / 2

.Lcase0B207:
	movs r0, #0
	b .LreturnB207
.Lcase1B207:
	movs r0, #0x10
	b .LreturnB207
.Lcase2B207:
	movs r0, #0x10
	b .LreturnB207
.Lcase3B207:
	movs r0, #0x10
	b .LreturnB207
.Lcase4B207:
	movs r0, #0x20
	b .LreturnB207
.Lcase22B207:
	movs r0, #4
	b .LreturnB207
.Lcase23B207:
	movs r0, #8
	b .LreturnB207
.Lcase24B207:
	movs r0, #0x10
	b .LreturnB207
.Lcase25B207:
	movs r0, #0x20
	b .LreturnB207
.Lcase5B207:
	movs r0, #0x20
	b .LreturnB207
.Lcase17B207:
	movs r0, #4
	b .LreturnB207
.Lcase19B207:
	movs r0, #8
	b .LreturnB207
.Lcase21B207:
	movs r0, #8
	b .LreturnB207
.Lcase26B207:
	movs r0, #0x18
	b .LreturnB207
.Lcase7B207:
	movs r0, #0x20
	b .LreturnB207
.Lcase15B207:
	movs r0, #8
	b .LreturnB207
.Lcase30B207:
	movs r0, #8
	b .LreturnB207
.Lcase31B207:
	movs r0, #8
	b .LreturnB207
.LdefaultB207:
	movs r0, #0
.LreturnB207:
	bx lr
	.fnend
	.size ScePafMisc_B207334F, . - ScePafMisc_B207334F

	.macro PafEnumSizeWrapper name, offset
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, lr}
	ldr r0, [r0, #\offset]
	bl ScePafMisc_B207334F
	pop {r4, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafEnumSizeWrapper ScePafMisc_24015182, 4
	PafEnumSizeWrapper ScePafMisc_BD8A2E25, 8

	.global ScePafMisc_2FEF71B0
	.type ScePafMisc_2FEF71B0, %function
	.thumb_func
ScePafMisc_2FEF71B0:
	.fnstart
	push {r4, r5, r6, lr}
	adds r4, r1, #0
	adds r5, r0, #0
	adds r0, r4, #0
	bl ScePafMisc_A48D0607
	adds r2, r0, #0
	adds r0, r5, #0
	ldr r1, [r0, #8]
	ldr r3, [r0, #0xc]
	mul r1, r1, r3
	ldr.w lr, [r4, #0x10]
	adds r0, r2, #0
	mul lr, r0, lr
	movs r3, #0
	adds r2, r3, #0
	adds r0, r5, #0
	cbz r1, .Ldone2FEF
	mov ip, r3
.Lcompare2FEF:
	cmp r3, r1
	bhs .Lincrement2FEF
	adds r4, r2, #0
	cmp r3, r1
	bhs .Lstore2FEF
	ldr r4, [r0]
	adds.w r4, r4, ip
.Lstore2FEF:
	str.w lr, [r4, #4]
.Lincrement2FEF:
	adds r3, #1
	add.w ip, ip, #8
	cmp r3, r1
	blo .Lcompare2FEF
.Ldone2FEF:
	movs r0, #1
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_2FEF71B0, . - ScePafMisc_2FEF71B0

	.global ScePafMisc_B7CA61AD
	.type ScePafMisc_B7CA61AD, %function
	.thumb_func
ScePafMisc_B7CA61AD:
	.fnstart
	push {r4, r5, r6, lr}
	ldr.w ip, [r1]
	movs r4, #0
	cmp.w ip, #0
	blt .LnotfoundB7
	ldr r3, [r1, #4]
	cmp r3, #0
	blt .LnotfoundB7
	ldr r1, [r0, #8]
	cmp ip, r1
	ldr r5, [r0, #0xc]
	bge .LnotfoundB7
	cmp r3, r5
	bge .LnotfoundB7
	cmp.w ip, #0
	blt .LwriteB7
	cmp r3, #0
	blt .LwriteB7
	cmp ip, r1
	bge .LwriteB7
	cmp r3, r5
	bge .LwriteB7
	mla r4, r3, r1, ip
	ldr r0, [r0]
	add.w r4, r0, r4, lsl #3
.LwriteB7:
	movs r0, #1
	str r2, [r4, #4]
	b .LreturnB7
.LnotfoundB7:
	adds r0, r4, #0
.LreturnB7:
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_B7CA61AD, . - ScePafMisc_B7CA61AD

	.macro PafEnumSizeInEights name, input_offset
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, lr}
	ldr r0, [r0, #\input_offset]
	bl ScePafMisc_B207334F
	lsrs r0, r0, #3
	pop {r4, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafEnumSizeInEights ScePafMisc_A48D0607, 8
	PafEnumSizeInEights ScePafMisc_C07E8C83, 4
