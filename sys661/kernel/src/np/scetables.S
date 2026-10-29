/* Generated PSP module import/export tables for sceNp */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	_sceKernelCheckThreadStack
	.type	_sceKernelCheckThreadStack, @function
_sceKernelCheckThreadStack: /* Kernel_Library 0xd13bde95 @ 0x4788 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMutex
	.type	sceKernelDeleteMutex, @function
sceKernelDeleteMutex: /* ThreadManForUser 0xf8170fbe @ 0x4790 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForUser 0x369ed59d @ 0x4798 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForUser 0x6b30100f @ 0x47a0 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForUser 0x82bc5777 @ 0x47a8 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForUser 0xb011b11f @ 0x47b0 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForUser 0xb7d098c6 @ 0x47b8 */
	jr	$ra
	nop
	.globl	sceNpCore_52440ABF
	.type	sceNpCore_52440ABF, @function
sceNpCore_52440ABF: /* sceNpCore 0x52440abf @ 0x47c0 */
	jr	$ra
	nop
	.globl	sceNpCore_57E15796
	.type	sceNpCore_57E15796, @function
sceNpCore_57E15796: /* sceNpCore 0x57e15796 @ 0x47c8 */
	jr	$ra
	nop
	.globl	sceRegCloseCategory
	.type	sceRegCloseCategory, @function
sceRegCloseCategory: /* sceReg 0x0cae832b @ 0x47d0 */
	jr	$ra
	nop
	.globl	sceRegOpenCategory
	.type	sceRegOpenCategory, @function
sceRegOpenCategory: /* sceReg 0x1d8a762e @ 0x47d8 */
	jr	$ra
	nop
	.globl	sceRegGetKeyValue
	.type	sceRegGetKeyValue, @function
sceRegGetKeyValue: /* sceReg 0x28a8e98a @ 0x47e0 */
	jr	$ra
	nop
	.globl	sceRegOpenRegistry
	.type	sceRegOpenRegistry, @function
sceRegOpenRegistry: /* sceReg 0x92e41280 @ 0x47e8 */
	jr	$ra
	nop
	.globl	sceRegGetKeyInfo
	.type	sceRegGetKeyInfo, @function
sceRegGetKeyInfo: /* sceReg 0xd4475aa8 @ 0x47f0 */
	jr	$ra
	nop
	.globl	sceRegCloseRegistry
	.type	sceRegCloseRegistry, @function
sceRegCloseRegistry: /* sceReg 0xfa8a5739 @ 0x47f8 */
	jr	$ra
	nop
	.globl	sceRtcGetCurrentTick
	.type	sceRtcGetCurrentTick, @function
sceRtcGetCurrentTick: /* sceRtc 0x3f7ad767 @ 0x4800 */
	jr	$ra
	nop

	.section .lib.ent.top, "a", @progbits
	.align	2
__lib_ent_top:
	.word	0

	.section .lib.ent, "a", @progbits
	.align	2
	.word	0
	.byte	0, 0
	.half	0x8000
	.byte	4, 2
	.half	0
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	36
	.word	.L_exp_tbl_1
	.word	0x00020000

	.section .lib.ent.btm, "a", @progbits
	.align	2
__lib_ent_btm:
	.word	0

	.section .lib.stub.top, "a", @progbits
	.align	2
__lib_stub_top:
	.word	0

	.section .lib.stub, "a", @progbits
	.align	2
	.word	.L_imp_lib_0
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_0
	.word	sceRtcGetCurrentTick
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_1
	.word	sceRegCloseCategory
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x4009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceNpCore_52440ABF
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	_sceKernelCheckThreadStack
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_4
	.word	sceKernelDeleteMutex

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x0006
	.byte	3, 1
	.asciz	"sceNp"
	.space	22
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceRtc"
	.space	5
.L_imp_lib_1:
	.asciz	"sceReg"
	.space	5
.L_imp_lib_2:
	.asciz	"sceNpCore"
	.space	6
.L_imp_lib_3:
	.asciz	"Kernel_Library"
	.space	5
.L_imp_lib_4:
	.asciz	"ThreadManForUser"
	.space	3
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceNp"
	.space	2
.L_exp_tbl_1:
	.word	0x02ca8caa
	.word	0x0b8bee48
	.word	0x1916432c
	.word	0x1d60ae4b
	.word	0x2d8e93d2
	.word	0x2e4769c3
	.word	0x37e1e274
	.word	0x4556a257
	.word	0x4b09907a
	.word	0x4b5c71c8
	.word	0x5fa879d8
	.word	0x60a4791a
	.word	0x633b5f71
	.word	0x64f72b22
	.word	0x66b86876
	.word	0x67bcf9e3
	.word	0x6999edc4
	.word	0x6cbb0614
	.word	0x7e0864df
	.word	0x857b47d3
	.word	0x9a264ef2
	.word	0x9b87b19b
	.word	0xa0be3c4b
	.word	0xb2bfadb2
	.word	0xb4063f7a
	.word	0xb819a0c8
	.word	0xbb069a87
	.word	0xc0b3616c
	.word	0xc250a650
	.word	0xc48f2847
	.word	0xcdcc21d3
	.word	0xcf83cc3b
	.word	0xe24da399
	.word	0xe41288e7
	.word	0xe90dfbc4
	.word	0xf2b95034
	.word	sceNp_02CA8CAA /* sceNp_02CA8CAA (0x02ca8caa) */
	.word	sceNp_0B8BEE48 /* sceNp_0B8BEE48 (0x0b8bee48) */
	.word	sceNp_1916432C /* sceNp_1916432C (0x1916432c) */
	.word	sceNp_1D60AE4B /* sceNp_1D60AE4B (0x1d60ae4b) */
	.word	sceNp_2D8E93D2 /* sceNp_2D8E93D2 (0x2d8e93d2) */
	.word	sceNp_2E4769C3 /* sceNp_2E4769C3 (0x2e4769c3) */
	.word	sceNp_37E1E274 /* sceNp_37E1E274 (0x37e1e274) */
	.word	sceNp_4556A257 /* sceNp_4556A257 (0x4556a257) */
	.word	sceNp_4B09907A /* sceNp_4B09907A (0x4b09907a) */
	.word	sceNp_4B5C71C8 /* sceNp_4B5C71C8 (0x4b5c71c8) */
	.word	sceNp_5FA879D8 /* sceNp_5FA879D8 (0x5fa879d8) */
	.word	sceNp_60A4791A /* sceNp_60A4791A (0x60a4791a) */
	.word	sceNp_633B5F71 /* sceNp_633B5F71 (0x633b5f71) */
	.word	sceNp_64F72B22 /* sceNp_64F72B22 (0x64f72b22) */
	.word	sceNp_66B86876 /* sceNp_66B86876 (0x66b86876) */
	.word	sceNp_67BCF9E3 /* sceNp_67BCF9E3 (0x67bcf9e3) */
	.word	sceNp_6999EDC4 /* sceNp_6999EDC4 (0x6999edc4) */
	.word	sceNp_6CBB0614 /* sceNp_6CBB0614 (0x6cbb0614) */
	.word	sceNp_7E0864DF /* sceNp_7E0864DF (0x7e0864df) */
	.word	sceNp_857B47D3 /* sceNp_857B47D3 (0x857b47d3) */
	.word	sceNp_9A264EF2 /* sceNp_9A264EF2 (0x9a264ef2) */
	.word	sceNp_9B87B19B /* sceNp_9B87B19B (0x9b87b19b) */
	.word	sceNp_A0BE3C4B /* sceNp_A0BE3C4B (0xa0be3c4b) */
	.word	sceNp_B2BFADB2 /* sceNp_B2BFADB2 (0xb2bfadb2) */
	.word	sceNp_B4063F7A /* sceNp_B4063F7A (0xb4063f7a) */
	.word	sceNp_B819A0C8 /* sceNp_B819A0C8 (0xb819a0c8) */
	.word	sceNp_BB069A87 /* sceNp_BB069A87 (0xbb069a87) */
	.word	sceNp_C0B3616C /* sceNp_C0B3616C (0xc0b3616c) */
	.word	sceNp_C250A650 /* sceNp_C250A650 (0xc250a650) */
	.word	sceNp_C48F2847 /* sceNp_C48F2847 (0xc48f2847) */
	.word	sceNp_CDCC21D3 /* sceNp_CDCC21D3 (0xcdcc21d3) */
	.word	sceNp_CF83CC3B /* sceNp_CF83CC3B (0xcf83cc3b) */
	.word	sceNp_E24DA399 /* sceNp_E24DA399 (0xe24da399) */
	.word	sceNp_E41288E7 /* sceNp_E41288E7 (0xe41288e7) */
	.word	sceNp_E90DFBC4 /* sceNp_E90DFBC4 (0xe90dfbc4) */
	.word	sceNp_F2B95034 /* sceNp_F2B95034 (0xf2b95034) */
	.word	0x00020000
	.word	0x00060004
	.word	0x000a0007
	.word	0x0012000b
	.word	0x00140013
	.word	0x00170016
	.word	0x0020001b
	.word	0x00230020

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x3f7ad767
.L_imp_nid_1:
	.word	0x0cae832b
	.word	0x1d8a762e
	.word	0x28a8e98a
	.word	0x92e41280
	.word	0xd4475aa8
	.word	0xfa8a5739
.L_imp_nid_2:
	.word	0x52440abf
	.word	0x57e15796
.L_imp_nid_3:
	.word	0xd13bde95
.L_imp_nid_4:
	.word	0xf8170fbe
	.word	0x369ed59d
	.word	0x6b30100f
	.word	0x82bc5777
	.word	0xb011b11f
	.word	0xb7d098c6

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
