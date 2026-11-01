/*
 * libSceMd5's compression (the SDK's static library, md5.o), hand-written in assembly by
 * Sony: the 64 steps run in one loop (md5_lop) that calls the round's function through ip;
 * each function is padded to 16 bytes, so the loop's exit (ip + 16) lands on the next
 * round's setup (init_F2 to init_F4) and, after F4, on md5_end. sl holds the round's four
 * rotations (as right rotations), r2 and r3 the message word's start and step.
 *
 * void _sceMd5Transform(SceUInt32 h[4], const SceUChar8 block[64]);
 */

	.syntax unified
	.cpu cortex-a9
	.fpu neon
	.thumb

	.section .text.sceMd5, "ax", %progbits
	.p2align 2
.global _sceMd5Transform
.type _sceMd5Transform, %function
.thumb_func
_sceMd5Transform:
	.fnstart
	push.w {r4, r5, r6, r7, r8, r9, sl, fp, lr}
	.save {r4, r5, r6, r7, r8, r9, sl, fp, lr}
	push {r0}
	.save {r0}
	ldr r4, [r0, #0]
	ldr r5, [r0, #4]
	ldr r6, [r0, #8]
	ldr r7, [r0, #12]
	adr.w ip, F1 + 1
	ldr.w r9, .Lmd5_tbl
	movs r2, #0
	movs r3, #4
	movw sl, #0x1419
	movt sl, #0x0a0f
md5_lop:
	ldr.w r8, [r1, r2]
	add r2, r3
	and.w r2, r2, #0x3c
	blx ip
	add r0, r8
	ldr.w r8, [r9], #4
	add r0, r8
	add r4, r0
	and.w r8, sl, #0x1f
	ror.w sl, sl, #8
	ror.w r4, r4, r8
	add r4, r5
	mov r0, r7
	mov r7, r6
	mov r6, r5
	mov r5, r4
	mov r4, r0
	tst.w r9, #0x3c
	bne.n md5_lop
	add.w ip, ip, #0x10
	bx ip

F1:	/* (b & c) | (~b & d) */
	eors.w r0, r6, r7
	ands r0, r5
	eors r0, r7
	bx lr
	nop
	nop
	nop
init_F2:
	movw sl, #0x171b
	movt sl, #0x0c12
	movs r2, #4
	movs r3, #20
	adr.w ip, F2 + 1
	b.n md5_lop
F2:	/* (b & d) | (c & ~d) */
	eors.w r0, r5, r6
	ands r0, r7
	eors r0, r6
	bx lr
	nop
	nop
	nop
init_F3:
	movw sl, #0x151c
	movt sl, #0x0910
	movs r2, #20
	movs r3, #12
	adr.w ip, F3 + 1
	b.n md5_lop
F3:	/* b ^ c ^ d */
	eors.w r0, r5, r6
	eors r0, r7
	bx lr
	nop
	nop
	nop
	nop
init_F4:
	movw sl, #0x161a
	movt sl, #0x0b11
	movs r2, #0
	movs r3, #28
	adr.w ip, F4 + 1
	b.n md5_lop
F4:	/* c ^ (b | ~d) */
	mvns r0, r7
	orrs r0, r5
	eors r0, r6
	bx lr
	nop
	nop
	nop
	nop
md5_end:
	pop {r0}
	ldr.w r8, [r0]
	ldr.w r9, [r0, #4]
	ldr.w sl, [r0, #8]
	ldr.w fp, [r0, #12]
	add r8, r4
	add r9, r5
	add sl, r6
	add fp, r7
	str.w r8, [r0]
	str.w r9, [r0, #4]
	str.w sl, [r0, #8]
	str.w fp, [r0, #12]
	pop.w {r4, r5, r6, r7, r8, r9, sl, fp, lr}
	bx lr
	.balign 4, 0
.Lmd5_tbl:
	.word md5_tbl
	.fnend
.size _sceMd5Transform, .-_sceMd5Transform

	/* the 64 additive constants, floor(abs(sin(i + 1)) * 2^32) */
	.section .rodata.sceMd5, "a", %progbits
	.p2align 6
.type md5_tbl, %object
md5_tbl:
	.word 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee
	.word 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501
	.word 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be
	.word 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821
	.word 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa
	.word 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8
	.word 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed
	.word 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a
	.word 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c
	.word 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70
	.word 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05
	.word 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665
	.word 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039
	.word 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1
	.word 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1
	.word 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
.size md5_tbl, .-md5_tbl

.section .note.GNU-stack, "", %progbits
