/*
 * sceGzipCrc32 (libdeflt.h), the SDK's libSceDeflt crc.o: hand-written NEON by Sony. It folds
 * 16 bytes at a time with vmull.p8 against the constants in front of the table, then finishes
 * byte by byte through the table (zlib's crc_table[0]). The two instructions in front of the
 * entry are the tail for fewer than 16 bytes, in the function's section.
 *
 * unsigned int sceGzipCrc32(unsigned int uiCrc, const unsigned char *pSrc, unsigned int uiSize);
 */

	.syntax unified
	.cpu cortex-a9
	.fpu neon
	.thumb

	.section .text.sceGzipCrc32, "ax", %progbits
	.p2align 1
	.thumb_func
.Lbytes:
	cmp r2, #0
	b.n .L7

	.global sceGzipCrc32
	.type sceGzipCrc32, %function
	.thumb_func
sceGzipCrc32:
	mvns r0, r0
	cmp r2, #0x10
	movw ip, #:lower16:s_crc32Table
	movt ip, #:upper16:s_crc32Table
	push {r0, r1, r4, lr}
	blo.n .Lbytes
	sub.w r3, ip, #0x10
	vldmia r3, {d27, d28}
	movs r4, #0
	vld1.8 {d0, d1}, [r1]!
	vmov d2, r0, r4
	vrev16.8 d26, d27
	vrev16.8 d29, d28
	subs r2, #0x20
	veor d0, d0, d2
	vrev32.16 q12, q13
	vrev32.16 q15, q14
	blo.n .L2
.L1:
	vmull.p8 q9, d1, d25
	vmull.p8 q11, d1, d27
	vmull.p8 q1, d0, d29
	vmull.p8 q3, d0, d31
	vmull.p8 q10, d1, d26
	vmull.p8 q2, d0, d30
	vmull.p8 q8, d1, d24
	vmull.p8 q0, d0, d28
	pld [r1, #0x40]
	veor q3, q3, q11
	subs r2, #0x10
	veor q1, q1, q9
	veor q2, q2, q10
	veor d6, d6, d7
	vuzp.16 d2, d3
	veor d2, d2, d3
	veor q0, q0, q8
	vshll.u16 q3, d6, #8
	vshll.u16 q1, d2, #8
	veor q2, q2, q3
	veor q1, q0, q1
	veor d4, d4, d5
	vadd.i64 q1, q1, q1
	vshll.u32 q2, d4, #0x11
	vld1.8 {d0, d1}, [r1]!
	veor q1, q1, q2
	veor d2, d2, d3
	veor d0, d0, d2
	bhs.n .L1
.L2:
	adds r2, #8
	blo.n .L3
	vmull.p8 q2, d1, d25
	vmull.p8 q8, d1, d27
	vmull.p8 q3, d1, d26
	vmull.p8 q1, d1, d24
	vmull.p8 q9, d0, d29
	vmull.p8 q10, d0, d31
	vmull.p8 q11, d0, d30
	vmull.p8 q15, d0, d28
	vld1.8 {d1}, [r1]!
	veor q2, q2, q9
	veor q8, q8, q10
	veor q3, q3, q11
	veor q1, q1, q15
	subs r2, #8
	b.n .L4
.L3:
	vmull.p8 q2, d0, d25
	vmull.p8 q8, d0, d27
	vmull.p8 q3, d0, d26
	vmull.p8 q1, d0, d24
.L4:
	vuzp.16 d4, d5
	add r1, r2
	veor d16, d16, d17
	lsls r2, r2, #3
	veor d4, d4, d5
	vdup.32 d18, r2
	rsbs r0, r2, #0
	vshll.u16 q8, d16, #8
	vshll.u16 q2, d4, #8
	veor q3, q3, q8
	veor q1, q1, q2
	veor d6, d6, d7
	vdup.32 q15, r0
	subs r0, #0x40
	vadd.i64 q1, q1, q1
	vshll.u32 q3, d6, #0x11
	vld1.8 {d28}, [r1]
	veor q3, q3, q1
	mov r1, sp
	veor d0, d6, d1
	vshl.u64 d1, d28, d18
	veor d0, d0, d7
	cbz r0, .L5
	vshl.u64 q14, q0, q15
	vmull.p8 q2, d28, d25
	vmull.p8 q8, d28, d27
	vorr.i32 d31, #0xc0
	vmull.p8 q1, d28, d24
	vmull.p8 q3, d28, d26
	vshl.u64 d28, d0, d31
	vuzp.16 d4, d5
	veor d16, d16, d17
	veor d4, d4, d5
	vshll.u16 q8, d16, #8
	vshll.u16 q2, d4, #8
	veor q3, q3, q8
	veor q1, q1, q2
	veor d6, d6, d7
	vadd.i64 q1, q1, q1
	vshll.u32 q3, d6, #0x11
	veor q1, q1, q14
	veor q3, q3, q1
	veor d0, d6, d7
	movs r0, #0
.L5:
	movs r2, #8
	vstr d0, [sp]
.L6:
	ldrb r3, [r1], #1
	and r4, r0, #0xff
	eors r4, r3
	ldr.w r3, [ip, r4, lsl #2]
	subs r2, #1
	eor.w r0, r3, r0, lsr #8
.L7:
	bne.n .L6
	mvns r0, r0
	add sp, #8
	pop {r4, pc}
.size sceGzipCrc32, .-sceGzipCrc32

/* the vmull.p8 multipliers, then the table (s_crc32Table) */
	.section .rodata, "a", %progbits
	.p2align 2
_s_crc32Table:
	.byte 0x66, 0x55, 0x00, 0x4f, 0xb1, 0xe6, 0xb0, 0x92
	.byte 0xe8, 0xcb, 0x8c, 0xba, 0x17, 0x25, 0x6a, 0xa0
s_crc32Table:
	.word 0x00000000, 0x77073096, 0xee0e612c, 0x990951ba
	.word 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3
	.word 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988
	.word 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91
	.word 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de
	.word 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7
	.word 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec
	.word 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5
	.word 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172
	.word 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b
	.word 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940
	.word 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59
	.word 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116
	.word 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f
	.word 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924
	.word 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d
	.word 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a
	.word 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433
	.word 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818
	.word 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01
	.word 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e
	.word 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457
	.word 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c
	.word 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65
	.word 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2
	.word 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb
	.word 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0
	.word 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9
	.word 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086
	.word 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f
	.word 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4
	.word 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad
	.word 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a
	.word 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683
	.word 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8
	.word 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1
	.word 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe
	.word 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7
	.word 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc
	.word 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5
	.word 0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252
	.word 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b
	.word 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60
	.word 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79
	.word 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236
	.word 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f
	.word 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04
	.word 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d
	.word 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a
	.word 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713
	.word 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38
	.word 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21
	.word 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e
	.word 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777
	.word 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c
	.word 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45
	.word 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2
	.word 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db
	.word 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0
	.word 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9
	.word 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6
	.word 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf
	.word 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94
	.word 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
	.size _s_crc32Table, .-_s_crc32Table

	.section .note.GNU-stack, "", %progbits
