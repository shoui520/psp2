/*
 * libSceSha384 (the SDK's static library, sha384.o): Sony's hand-written SHA-384
 * compression core (_sceSha384Core) and round constants (key_value).
 */
	.syntax unified
	.arch armv7-a
	.cpu cortex-a9
	.fpu neon
	.thumb

	.section .text.sceSha384, "ax", %progbits
	.align 1
	.global _sceSha384Core
	.thumb_func
	.type _sceSha384Core, %function
_sceSha384Core:
	.fnstart
	.pad #640
	sub.w	sp, sp, #640
	mov	r2, sp
	vldmia	r0!, {d0-d7}
	vstmia	r2!, {d0-d7}
	vldmia	r0!, {d0-d7}
	vstmia	r2!, {d0-d7}
	mov.w	r3, #64
sha384_w16_79_lop:
	vldr	d0, [r2, #-128]
	vldr	d1, [r2, #-120]
	vldr	d2, [r2, #-56]
	vldr	d3, [r2, #-16]
	vadd.i64	d0, d0, d2
	vshr.u64	d16, d1, #1
	vshl.s64	d17, d1, #63
	vorr	d4, d16, d17
	vshr.u64	d16, d1, #8
	vshl.s64	d17, d1, #56
	vorr	d16, d16, d17
	veor	d4, d4, d16
	vshr.u64	d16, d1, #7
	veor	d4, d4, d16
	vadd.i64	d0, d0, d4
	vshr.u64	d16, d3, #19
	vshl.s64	d17, d3, #45
	vorr	d4, d16, d17
	vshr.u64	d16, d3, #61
	vshl.s64	d17, d3, #3
	vorr	d16, d16, d17
	veor	d4, d4, d16
	vshr.u64	d16, d3, #6
	veor	d4, d4, d16
	vadd.i64	d0, d0, d4
	vstr	d0, [r2]
	add.w	r2, r2, #8
	adds.w	r3, r3, #-1
	bne.n	sha384_w16_79_lop
	vldmia	r1, {d0-d7}
	mov	r0, sp
	movw	r2, #:lower16:key_value
	movt	r2, #:upper16:key_value
	mov.w	r3, #80
sha384_round8_lop:
	vshr.u64	d16, d4, #14
	vshl.s64	d17, d4, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d4, #18
	vshl.s64	d17, d4, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d4, #41
	vshl.s64	d17, d4, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d7, d18
	veor	d19, d5, d6
	vand	d19, d19, d4
	veor	d19, d19, d6
	vadd.i64	d18, d18, d19
	vldr	d19, [r2]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0]
	vadd.i64	d18, d18, d19
	vadd.i64	d3, d3, d18
	vshr.u64	d16, d0, #28
	vshl.s64	d17, d0, #36
	vorr	d7, d16, d17
	vshr.u64	d16, d0, #34
	vshl.s64	d17, d0, #30
	vorr	d16, d16, d17
	veor	d7, d7, d16
	vshr.u64	d16, d0, #39
	vshl.s64	d17, d0, #25
	vorr	d16, d16, d17
	veor	d7, d7, d16
	vorr	d19, d0, d1
	vand	d19, d19, d2
	vand	d16, d0, d1
	vorr	d19, d19, d16
	vadd.i64	d7, d7, d19
	vadd.i64	d7, d7, d18
	vshr.u64	d16, d3, #14
	vshl.s64	d17, d3, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d3, #18
	vshl.s64	d17, d3, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d3, #41
	vshl.s64	d17, d3, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d6, d18
	veor	d19, d4, d5
	vand	d19, d19, d3
	veor	d19, d19, d5
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #8]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #8]
	vadd.i64	d18, d18, d19
	vadd.i64	d2, d2, d18
	vshr.u64	d16, d7, #28
	vshl.s64	d17, d7, #36
	vorr	d6, d16, d17
	vshr.u64	d16, d7, #34
	vshl.s64	d17, d7, #30
	vorr	d16, d16, d17
	veor	d6, d6, d16
	vshr.u64	d16, d7, #39
	vshl.s64	d17, d7, #25
	vorr	d16, d16, d17
	veor	d6, d6, d16
	vorr	d19, d7, d0
	vand	d19, d19, d1
	vand	d16, d7, d0
	vorr	d19, d19, d16
	vadd.i64	d6, d6, d19
	vadd.i64	d6, d6, d18
	vshr.u64	d16, d2, #14
	vshl.s64	d17, d2, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d2, #18
	vshl.s64	d17, d2, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d2, #41
	vshl.s64	d17, d2, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d5, d18
	veor	d19, d3, d4
	vand	d19, d19, d2
	veor	d19, d19, d4
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #16]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #16]
	vadd.i64	d18, d18, d19
	vadd.i64	d1, d1, d18
	vshr.u64	d16, d6, #28
	vshl.s64	d17, d6, #36
	vorr	d5, d16, d17
	vshr.u64	d16, d6, #34
	vshl.s64	d17, d6, #30
	vorr	d16, d16, d17
	veor	d5, d5, d16
	vshr.u64	d16, d6, #39
	vshl.s64	d17, d6, #25
	vorr	d16, d16, d17
	veor	d5, d5, d16
	vorr	d19, d6, d7
	vand	d19, d19, d0
	vand	d16, d6, d7
	vorr	d19, d19, d16
	vadd.i64	d5, d5, d19
	vadd.i64	d5, d5, d18
	vshr.u64	d16, d1, #14
	vshl.s64	d17, d1, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d1, #18
	vshl.s64	d17, d1, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d1, #41
	vshl.s64	d17, d1, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d4, d18
	veor	d19, d2, d3
	vand	d19, d19, d1
	veor	d19, d19, d3
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #24]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #24]
	vadd.i64	d18, d18, d19
	vadd.i64	d0, d0, d18
	vshr.u64	d16, d5, #28
	vshl.s64	d17, d5, #36
	vorr	d4, d16, d17
	vshr.u64	d16, d5, #34
	vshl.s64	d17, d5, #30
	vorr	d16, d16, d17
	veor	d4, d4, d16
	vshr.u64	d16, d5, #39
	vshl.s64	d17, d5, #25
	vorr	d16, d16, d17
	veor	d4, d4, d16
	vorr	d19, d5, d6
	vand	d19, d19, d7
	vand	d16, d5, d6
	vorr	d19, d19, d16
	vadd.i64	d4, d4, d19
	vadd.i64	d4, d4, d18
	vshr.u64	d16, d0, #14
	vshl.s64	d17, d0, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d0, #18
	vshl.s64	d17, d0, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d0, #41
	vshl.s64	d17, d0, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d3, d18
	veor	d19, d1, d2
	vand	d19, d19, d0
	veor	d19, d19, d2
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #32]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #32]
	vadd.i64	d18, d18, d19
	vadd.i64	d7, d7, d18
	vshr.u64	d16, d4, #28
	vshl.s64	d17, d4, #36
	vorr	d3, d16, d17
	vshr.u64	d16, d4, #34
	vshl.s64	d17, d4, #30
	vorr	d16, d16, d17
	veor	d3, d3, d16
	vshr.u64	d16, d4, #39
	vshl.s64	d17, d4, #25
	vorr	d16, d16, d17
	veor	d3, d3, d16
	vorr	d19, d4, d5
	vand	d19, d19, d6
	vand	d16, d4, d5
	vorr	d19, d19, d16
	vadd.i64	d3, d3, d19
	vadd.i64	d3, d3, d18
	vshr.u64	d16, d7, #14
	vshl.s64	d17, d7, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d7, #18
	vshl.s64	d17, d7, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d7, #41
	vshl.s64	d17, d7, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d2, d18
	veor	d19, d0, d1
	vand	d19, d19, d7
	veor	d19, d19, d1
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #40]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #40]
	vadd.i64	d18, d18, d19
	vadd.i64	d6, d6, d18
	vshr.u64	d16, d3, #28
	vshl.s64	d17, d3, #36
	vorr	d2, d16, d17
	vshr.u64	d16, d3, #34
	vshl.s64	d17, d3, #30
	vorr	d16, d16, d17
	veor	d2, d2, d16
	vshr.u64	d16, d3, #39
	vshl.s64	d17, d3, #25
	vorr	d16, d16, d17
	veor	d2, d2, d16
	vorr	d19, d3, d4
	vand	d19, d19, d5
	vand	d16, d3, d4
	vorr	d19, d19, d16
	vadd.i64	d2, d2, d19
	vadd.i64	d2, d2, d18
	vshr.u64	d16, d6, #14
	vshl.s64	d17, d6, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d6, #18
	vshl.s64	d17, d6, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d6, #41
	vshl.s64	d17, d6, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d1, d18
	veor	d19, d7, d0
	vand	d19, d19, d6
	veor	d19, d19, d0
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #48]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #48]
	vadd.i64	d18, d18, d19
	vadd.i64	d5, d5, d18
	vshr.u64	d16, d2, #28
	vshl.s64	d17, d2, #36
	vorr	d1, d16, d17
	vshr.u64	d16, d2, #34
	vshl.s64	d17, d2, #30
	vorr	d16, d16, d17
	veor	d1, d1, d16
	vshr.u64	d16, d2, #39
	vshl.s64	d17, d2, #25
	vorr	d16, d16, d17
	veor	d1, d1, d16
	vorr	d19, d2, d3
	vand	d19, d19, d4
	vand	d16, d2, d3
	vorr	d19, d19, d16
	vadd.i64	d1, d1, d19
	vadd.i64	d1, d1, d18
	vshr.u64	d16, d5, #14
	vshl.s64	d17, d5, #50
	vorr	d18, d16, d17
	vshr.u64	d16, d5, #18
	vshl.s64	d17, d5, #46
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vshr.u64	d16, d5, #41
	vshl.s64	d17, d5, #23
	vorr	d16, d16, d17
	veor	d18, d18, d16
	vadd.i64	d18, d0, d18
	veor	d19, d6, d7
	vand	d19, d19, d5
	veor	d19, d19, d7
	vadd.i64	d18, d18, d19
	vldr	d19, [r2, #56]
	vadd.i64	d18, d18, d19
	vldr	d19, [r0, #56]
	vadd.i64	d18, d18, d19
	vadd.i64	d4, d4, d18
	vshr.u64	d16, d1, #28
	vshl.s64	d17, d1, #36
	vorr	d0, d16, d17
	vshr.u64	d16, d1, #34
	vshl.s64	d17, d1, #30
	vorr	d16, d16, d17
	veor	d0, d0, d16
	vshr.u64	d16, d1, #39
	vshl.s64	d17, d1, #25
	vorr	d16, d16, d17
	veor	d0, d0, d16
	vorr	d19, d1, d2
	vand	d19, d19, d3
	vand	d16, d1, d2
	vorr	d19, d19, d16
	vadd.i64	d0, d0, d19
	vadd.i64	d0, d0, d18
	add.w	r0, r0, #64
	add.w	r2, r2, #64
	subs.w	r3, r3, #8
	bne.w	sha384_round8_lop
	vldmia	r1, {d16-d23}
	vadd.i64	q0, q0, q8
	vadd.i64	q1, q1, q9
	vadd.i64	q2, q2, q10
	vadd.i64	q3, q3, q11
	vstmia	r1, {d0-d7}
	add.w	sp, sp, #640
	bx	lr
	.fnend
	.size _sceSha384Core, .-_sceSha384Core

	.section .rodata.sceSha384, "a", %progbits
	.align 3
	.type key_value, %object
	.size key_value, 640
key_value:
	.word 0xd728ae22, 0x428a2f98, 0x23ef65cd, 0x71374491
	.word 0xec4d3b2f, 0xb5c0fbcf, 0x8189dbbc, 0xe9b5dba5
	.word 0xf348b538, 0x3956c25b, 0xb605d019, 0x59f111f1
	.word 0xaf194f9b, 0x923f82a4, 0xda6d8118, 0xab1c5ed5
	.word 0xa3030242, 0xd807aa98, 0x45706fbe, 0x12835b01
	.word 0x4ee4b28c, 0x243185be, 0xd5ffb4e2, 0x550c7dc3
	.word 0xf27b896f, 0x72be5d74, 0x3b1696b1, 0x80deb1fe
	.word 0x25c71235, 0x9bdc06a7, 0xcf692694, 0xc19bf174
	.word 0x9ef14ad2, 0xe49b69c1, 0x384f25e3, 0xefbe4786
	.word 0x8b8cd5b5, 0x0fc19dc6, 0x77ac9c65, 0x240ca1cc
	.word 0x592b0275, 0x2de92c6f, 0x6ea6e483, 0x4a7484aa
	.word 0xbd41fbd4, 0x5cb0a9dc, 0x831153b5, 0x76f988da
	.word 0xee66dfab, 0x983e5152, 0x2db43210, 0xa831c66d
	.word 0x98fb213f, 0xb00327c8, 0xbeef0ee4, 0xbf597fc7
	.word 0x3da88fc2, 0xc6e00bf3, 0x930aa725, 0xd5a79147
	.word 0xe003826f, 0x06ca6351, 0x0a0e6e70, 0x14292967
	.word 0x46d22ffc, 0x27b70a85, 0x5c26c926, 0x2e1b2138
	.word 0x5ac42aed, 0x4d2c6dfc, 0x9d95b3df, 0x53380d13
	.word 0x8baf63de, 0x650a7354, 0x3c77b2a8, 0x766a0abb
	.word 0x47edaee6, 0x81c2c92e, 0x1482353b, 0x92722c85
	.word 0x4cf10364, 0xa2bfe8a1, 0xbc423001, 0xa81a664b
	.word 0xd0f89791, 0xc24b8b70, 0x0654be30, 0xc76c51a3
	.word 0xd6ef5218, 0xd192e819, 0x5565a910, 0xd6990624
	.word 0x5771202a, 0xf40e3585, 0x32bbd1b8, 0x106aa070
	.word 0xb8d2d0c8, 0x19a4c116, 0x5141ab53, 0x1e376c08
	.word 0xdf8eeb99, 0x2748774c, 0xe19b48a8, 0x34b0bcb5
	.word 0xc5c95a63, 0x391c0cb3, 0xe3418acb, 0x4ed8aa4a
	.word 0x7763e373, 0x5b9cca4f, 0xd6b2b8a3, 0x682e6ff3
	.word 0x5defb2fc, 0x748f82ee, 0x43172f60, 0x78a5636f
	.word 0xa1f0ab72, 0x84c87814, 0x1a6439ec, 0x8cc70208
	.word 0x23631e28, 0x90befffa, 0xde82bde9, 0xa4506ceb
	.word 0xb2c67915, 0xbef9a3f7, 0xe372532b, 0xc67178f2
	.word 0xea26619c, 0xca273ece, 0x21c0c207, 0xd186b8c7
	.word 0xcde0eb1e, 0xeada7dd6, 0xee6ed178, 0xf57d4f7f
	.word 0x72176fba, 0x06f067aa, 0xa2c898a6, 0x0a637dc5
	.word 0xbef90dae, 0x113f9804, 0x131c471b, 0x1b710b35
	.word 0x23047d84, 0x28db77f5, 0x40c72493, 0x32caab7b
	.word 0x15c9bebc, 0x3c9ebe0a, 0x9c100d4c, 0x431d67c4
	.word 0xcb3e42b6, 0x4cc5d4be, 0xfc657e2a, 0x597f299c
	.word 0x3ad6faec, 0x5fcb6fab, 0x4a475817, 0x6c44198c
