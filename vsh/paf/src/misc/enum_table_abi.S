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
