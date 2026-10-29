/* Generated PSP module import/export tables for sceNetAdhocDiscover_Library */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	_sceKernelCheckThreadStack
	.type	_sceKernelCheckThreadStack, @function
_sceKernelCheckThreadStack: /* Kernel_Library 0xd13bde95 @ 0x0d74 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* Kernel_Library 0x1839852a @ 0x0d7c */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForUser 0x82bc5777 @ 0x0d84 */
	jr	$ra
	nop
	.globl	sceNetAdhocctlScan
	.type	sceNetAdhocctlScan, @function
sceNetAdhocctlScan: /* sceNetAdhocctl 0x08fff7a0 @ 0x0d8c */
	jr	$ra
	nop
	.globl	sceNetAdhocctlDisconnect
	.type	sceNetAdhocctlDisconnect, @function
sceNetAdhocctlDisconnect: /* sceNetAdhocctl 0x34401d65 @ 0x0d94 */
	jr	$ra
	nop
	.globl	sceNetAdhocctlJoin
	.type	sceNetAdhocctlJoin, @function
sceNetAdhocctlJoin: /* sceNetAdhocctl 0x5e7f79c9 @ 0x0d9c */
	jr	$ra
	nop
	.globl	sceNetAdhocctlGetScanInfo
	.type	sceNetAdhocctlGetScanInfo, @function
sceNetAdhocctlGetScanInfo: /* sceNetAdhocctl 0x81aee1be @ 0x0da4 */
	jr	$ra
	nop
	.globl	sceNetAdhocctlGetPeerList
	.type	sceNetAdhocctlGetPeerList, @function
sceNetAdhocctlGetPeerList: /* sceNetAdhocctl 0xe162cb14 @ 0x0dac */
	jr	$ra
	nop
	.globl	sceNetAdhocctlCreate
	.type	sceNetAdhocctlCreate, @function
sceNetAdhocctlCreate: /* sceNetAdhocctl 0xec0635c1 @ 0x0db4 */
	jr	$ra
	nop
	.globl	sceNetAdhocctl_lib_1572422C
	.type	sceNetAdhocctl_lib_1572422C, @function
sceNetAdhocctl_lib_1572422C: /* sceNetAdhocctl_lib 0x1572422c @ 0x0dbc */
	jr	$ra
	nop
	.globl	sceNetAdhocctl_lib_1A2B6D03
	.type	sceNetAdhocctl_lib_1A2B6D03, @function
sceNetAdhocctl_lib_1A2B6D03: /* sceNetAdhocctl_lib 0x1a2b6d03 @ 0x0dc4 */
	jr	$ra
	nop
	.globl	sceNetAdhocctl_lib_1C679240
	.type	sceNetAdhocctl_lib_1C679240, @function
sceNetAdhocctl_lib_1C679240: /* sceNetAdhocctl_lib 0x1c679240 @ 0x0dcc */
	jr	$ra
	nop
	.globl	sceNetAdhocctl_lib_F6057CC4
	.type	sceNetAdhocctl_lib_F6057CC4, @function
sceNetAdhocctl_lib_F6057CC4: /* sceNetAdhocctl_lib 0xf6057cc4 @ 0x0dd4 */
	jr	$ra
	nop
	.globl	sceNetAdhocctl_lib_F8BABD85
	.type	sceNetAdhocctl_lib_F8BABD85, @function
sceNetAdhocctl_lib_F8BABD85: /* sceNetAdhocctl_lib 0xf8babd85 @ 0x0ddc */
	jr	$ra
	nop
	.globl	sceNetIfhandle_lib_15CFE3C0
	.type	sceNetIfhandle_lib_15CFE3C0, @function
sceNetIfhandle_lib_15CFE3C0: /* sceNetIfhandle_lib 0x15cfe3c0 @ 0x0de4 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_lib_76BAD213
	.type	sceNetIfhandle_lib_76BAD213, @function
sceNetIfhandle_lib_76BAD213: /* sceNetIfhandle_lib 0x76bad213 @ 0x0dec */
	jr	$ra
	nop
	.globl	sceNet_lib_1858883D
	.type	sceNet_lib_1858883D, @function
sceNet_lib_1858883D: /* sceNet_lib 0x1858883d @ 0x0df4 */
	jr	$ra
	nop
	.globl	sceNet_lib_E0A81C7C
	.type	sceNet_lib_E0A81C7C, @function
sceNet_lib_E0A81C7C: /* sceNet_lib 0xe0a81c7c @ 0x0dfc */
	jr	$ra
	nop
	.globl	sceUtilityGetSystemParamInt
	.type	sceUtilityGetSystemParamInt, @function
sceUtilityGetSystemParamInt: /* sceUtility 0xa5da2406 @ 0x0e04 */
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
	.byte	4, 0
	.half	6
	.word	.L_exp_tbl_1

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
	.word	sceUtilityGetSystemParamInt
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x4009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceNetIfhandle_lib_15CFE3C0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceNet_lib_1858883D
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_3
	.word	sceNetAdhocctlScan
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_4
	.word	sceNetAdhocctl_lib_1572422C
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_5
	.word	_sceKernelCheckThreadStack
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_6
	.word	sceKernelGetSystemTimeWide

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
	.byte	2, 1
	.asciz	"sceNetAdhocDiscover_Library"
	.space	0
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceUtility"
	.space	5
.L_imp_lib_1:
	.asciz	"sceNetIfhandle_lib"
	.space	5
.L_imp_lib_2:
	.asciz	"sceNet_lib"
	.space	5
.L_imp_lib_3:
	.asciz	"sceNetAdhocctl"
	.space	5
.L_imp_lib_4:
	.asciz	"sceNetAdhocctl_lib"
	.space	5
.L_imp_lib_5:
	.asciz	"Kernel_Library"
	.space	5
.L_imp_lib_6:
	.asciz	"ThreadManForUser"
	.space	3
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceNetAdhocDiscover"
	.align	2
.L_exp_tbl_1:
	.word	0x52de1b97
	.word	0x941b3877
	.word	0x944ddbc6
	.word	0xa2246614
	.word	0xa423a21b
	.word	0xf7d13214
	.word	sceNetAdhocDiscover_52DE1B97 /* sceNetAdhocDiscover_52DE1B97 (0x52de1b97) */
	.word	sceNetAdhocDiscover_941B3877 /* sceNetAdhocDiscover_941B3877 (0x941b3877) */
	.word	sceNetAdhocDiscover_944DDBC6 /* sceNetAdhocDiscover_944DDBC6 (0x944ddbc6) */
	.word	sceNetAdhocDiscover_A2246614 /* sceNetAdhocDiscover_A2246614 (0xa2246614) */
	.word	sceNetAdhocDiscover_A423A21B /* sceNetAdhocDiscover_A423A21B (0xa423a21b) */
	.word	sceNetAdhocDiscover_F7D13214 /* sceNetAdhocDiscover_F7D13214 (0xf7d13214) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xa5da2406
.L_imp_nid_1:
	.word	0x15cfe3c0
	.word	0x76bad213
.L_imp_nid_2:
	.word	0x1858883d
	.word	0xe0a81c7c
.L_imp_nid_3:
	.word	0x08fff7a0
	.word	0x34401d65
	.word	0x5e7f79c9
	.word	0x81aee1be
	.word	0xe162cb14
	.word	0xec0635c1
.L_imp_nid_4:
	.word	0x1572422c
	.word	0x1a2b6d03
	.word	0x1c679240
	.word	0xf6057cc4
	.word	0xf8babd85
.L_imp_nid_5:
	.word	0xd13bde95
	.word	0x1839852a
.L_imp_nid_6:
	.word	0x82bc5777

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
