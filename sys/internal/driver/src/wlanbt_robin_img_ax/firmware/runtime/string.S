/*
 * The C library's string routines (RVCT's hand-written ones), instruction for instruction as the
 * original has them: memcmp (0x00052234) and strcpy (0x00052360) in Thumb, strncpy (0x000523b0)
 * and strcmp (0x00052420) in ARM. Each takes a word at a time while both pointers are aligned
 * (strcpy, strncpy and strcmp test the word for a zero byte with the 0x01010101 trick), a byte
 * at a time otherwise. strcmp's result for words that differ is the deciding byte's difference
 * shifted to the top, halved, the carry in bit 31; byte by byte it is the bytes' difference.
 * __aeabi_memmove (0x0005228c) is in boot/reset.S, beside __aeabi_memmove4, the word copy it
 * branches into. The filename is inferred.
 */
	.syntax unified
	.arch armv5te

	.text

/* memcmp (0x00052234) */
	.thumb
	.global memcmp
	.thumb_func
	.type	memcmp, %function
memcmp:
	push	{r4}
	cmp	r2, #4
	bcc	3f
	movs	r3, r0
	orrs	r3, r1
	lsls	r3, r3, #30
	bne	3f
1:
	ldmia	r0!, {r3}
	ldmia	r1!, {r4}
	cmp	r3, r4
	bne	2f
	subs	r2, r2, #4
	cmp	r2, #4
	bcs	1b
2:
	cmp	r3, r4
	beq	3f
	subs	r0, r0, #4
	subs	r1, r1, #4
3:
	cmp	r2, #0
	bne	5f
	movs	r0, #0
4:
	pop	{r4}
	bx	lr
5:
	lsls	r3, r2, #31
	beq	6f
	adds	r2, r2, #1
	b	7f
6:
	ldrb	r3, [r0]
	ldrb	r4, [r1]
	adds	r0, r0, #1
	adds	r1, r1, #1
	cmp	r3, r4
	bne	8f
7:
	ldrb	r3, [r0]
	ldrb	r4, [r1]
	adds	r0, r0, #1
	adds	r1, r1, #1
	cmp	r3, r4
	bne	8f
	subs	r2, r2, #2
	bne	6b
8:
	subs	r0, r3, r4
	b	4b
	.balign	4, 0

/* strcpy (0x00052360) */
	.global strcpy
	.thumb_func
	.type	strcpy, %function
strcpy:
	movs	r3, r0
	orrs	r3, r1
	movs	r2, r0
	push	{r4, r5, r6}
	lsls	r3, r3, #30
	bne	4f
	ldr	r4, .Lstrcpy_ones
	lsls	r5, r4, #7
	b	2f
1:
	stmia	r2!, {r3}
2:
	ldmia	r1!, {r3}
	subs	r6, r3, r4
	bics	r6, r3
	tst	r6, r5
	beq	1b
3:
	lsls	r1, r3, #24
	lsrs	r1, r1, #24
	strb	r1, [r2]
	adds	r2, r2, #1
	cmp	r1, #0
	beq	5f
	lsrs	r3, r3, #8
	b	3b
4:
	ldrb	r4, [r1]
	adds	r1, r1, #1
	adds	r3, r2, #1
	cmp	r4, #0
	strb	r4, [r2]
	beq	5f
	ldrb	r4, [r1]
	adds	r1, r1, #1
	adds	r2, r3, #1
	cmp	r4, #0
	strb	r4, [r3]
	bne	4b
5:
	pop	{r4, r5, r6}
	bx	lr
	.balign	4, 0
.Lstrcpy_ones:
	.word	0x01010101

	.arm
/* strncpy (0x000523b0): copies up to r2 bytes; the rest of the r2 after the terminator is cleared. */
	.global strncpy
	.type	strncpy, %function
strncpy:
	tst	r0, #3
	push	{r4, lr}
	mov	r4, r0
	tsteq	r1, #3
	bne	3f
	ldr	lr, .Lstrncpy_ones
1:
	subs	r2, r2, #4
	blt	2f
	ldr	r3, [r1], #4
	sub	ip, r3, lr
	bic	ip, ip, r3
	ands	ip, ip, lr, lsl #7
	streq	r3, [r0], #4
	beq	1b
	sub	r1, r1, #4
2:
	add	r2, r2, #4
3:
	subs	r2, r2, #1
	blt	4f
	ldrb	r3, [r1], #1
	cmp	r3, #0
	strb	r3, [r0], #1
	bne	3b
	mov	r1, r2
	bl	__aeabi_memclr
4:
	mov	r0, r4
	pop	{r4, lr}
	bx	lr
.Lstrncpy_ones:
	.word	0x01010101

/* strcmp (0x00052420) */
	.global strcmp
	.type	strcmp, %function
strcmp:
	tst	r0, #3
	tsteq	r1, #3
	bne	3f
	stmdb	sp!, {lr}
	ldr	lr, .Lstrcmp_ones
1:
	ldr	r2, [r0], #4
	ldr	r3, [r1], #4
	sub	ip, r2, lr
	bic	ip, ip, r2
	ands	ip, ip, lr, lsl #7
	cmpeq	r2, r3
	beq	1b
	ldmia	sp!, {lr}
	lsl	r0, r3, #24
	subs	r0, r0, r2, lsl #24
	tsteq	ip, #0xff0
	bne	2f
	lsl	r0, r3, #16
	subs	r0, r0, r2, lsl #16
	tsteq	ip, #0xff00
	bne	2f
	lsl	r0, r3, #8
	subs	r0, r0, r2, lsl #8
	tsteq	ip, #0xff0000
	subseq	r0, r3, r2
2:
	rrxne	r0, r0
	bx	lr
3:
	ldrb	r2, [r0], #1
	ldrb	r3, [r1], #1
	cmp	r2, #1
	cmpcs	r2, r3
	bne	4f
	ldrb	r2, [r0], #1
	ldrb	r3, [r1], #1
	cmp	r2, #1
	cmpcs	r2, r3
	beq	3b
4:
	sub	r0, r2, r3
	bx	lr
.Lstrcmp_ones:
	.word	0x01010101
