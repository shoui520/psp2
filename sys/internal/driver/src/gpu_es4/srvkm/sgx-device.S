/* Actual default SGX callbacks, buffer-manager and runtime dependencies. */
.syntax unified
.thumb
.text
.align 2
.global SGXRegisterDevice
.type SGXRegisterDevice, %function
.thumb_func
SGXRegisterDevice:
	ldr r2, SGXRegisterDeviceLiteral2f8
	movs r3, #7
	push.w {r4, r5, r6, r7, r8, lr}
	mov r4, r0
	str r3, [r0]
	movs r3, #0
	mov.w r1, #0x370
	str r2, [r0, #0x18]
	ldr r2, SGXRegisterDeviceLiteral2fc
	str r3, [r0, #4]
	str r2, [r0, #0x1c]
	ldr r2, SGXRegisterDeviceLiteral300
	strd r2, r3, [r0, #0x20]
	ldr r2, SGXRegisterDeviceLiteral304
	str r2, [r0, #0x28]
	ldr r2, SGXRegisterDeviceLiteral308
	str r2, [r0, #0x34]
	ldr r2, SGXRegisterDeviceLiteral30c
	str r2, [r0, #0x38]
	ldr r2, SGXRegisterDeviceLiteral310
	strd r2, r3, [r0, #0x40]
	movs r0, #1
	add.w r2, r4, #0x4c
	bl GpuAllocFlags
	mov r5, r0
	cmp r0, #0
	bne.w .LSGXRegisterDevice2f2
	ldr r0, [r4, #0x4c]
	mov r1, r5
	mov.w r2, #0x370
	movw r6, #0x121
	bl OSMemSet
	ldr r3, [r4, #0x4c]
	movs r0, #1
	movt r0, #0x700
	lsls r6, r6, #9
	mov.w r2, #0x7000000
	str r0, [r3, #0x28]
	mov.w r0, #0x50000000
	str r0, [r3, #0x34]
	movw r0, #0xcfff
	lsls r0, r0, #0xc
	str r2, [r3]
	mov.w r2, #0x40000000
	strd r6, r5, [r3, #0x3c]
	str r0, [r3, #0x38]
	movs r0, #2
	movt r0, #0x700
	str r2, [r3, #0xc]
	str r0, [r3, #0x50]
	mov.w r0, #0x14000000
	str r0, [r3, #0x5c]
	mov.w r0, #0x7f000
	strd r0, r6, [r3, #0x60]
	movs r0, #3
	mov r7, r0
	movw r2, #0xffff
	movt r7, #0x700
	lsls r2, r2, #0xc
	str r7, [r3, #0x78]
	mov.w r7, #0x1c000000
	str.w r7, [r3, #0x84]
	movw r7, #0x3fff
	lsls r7, r7, #0xc
	mov.w lr, #5
	movt lr, #0x700
	str r2, [r3, #0x10]
	strd r7, r6, [r3, #0x88]
	movs r7, #4
	movt r7, #0x700
	mov.w r2, #0x1000
	str.w r7, [r3, #0xa0]
	mov.w r7, #0x30000000
	mov.w r1, #0x14200
	str.w r7, [r3, #0xac]
	str.w lr, [r3, #0xc8]
	mov.w r7, #0x2000000
	mov.w lr, #0x34000000
	strd r1, r5, [r3, #0x14]
	strd r7, r1, [r3, #0xb0]
	mov.w r8, #0x10
	str r2, [r3, #0x20]
	mov.w ip, #0x18000000
	str r2, [r3, #0x48]
	movt r8, #0x700
	str r0, [r3, #0x68]
	str r2, [r3, #0x70]
	str.w r0, [r3, #0x90]
	str.w r2, [r3, #0x98]
	str.w r5, [r3, #0xb8]
	str.w r2, [r3, #0xc0]
	strd lr, r7, [r3, #0xd4]
	movs r7, #6
	movt r7, #0x700
	mov.w lr, #0xa
	str.w r7, [r3, #0xf0]
	mov.w r7, #0x10000000
	str.w r7, [r3, #0xfc]
	movw r7, #0x1fff
	lsls r7, r7, #0xc
	movt lr, #0x700
	str.w lr, [r3, #0x190]
	mov.w lr, #0x20000000
	strd r7, r1, [r3, #0x100]
	movs r7, #7
	movt r7, #0x700
	strd r1, r5, [r3, #0xdc]
	str.w r7, [r3, #0x118]
	movw r7, #0x1bff
	lsls r7, r7, #0xc
	strd r1, r5, [r3, #0x1a4]
	str.w r2, [r3, #0xe8]
	strd r7, r1, [r3, #0x128]
	movs r7, #8
	movt r7, #0x700
	str.w r5, [r3, #0x108]
	str.w r7, [r3, #0x140]
	mov.w r7, #0xc000000
	str.w r7, [r3, #0x14c]
	movw r7, #0xfff
	lsls r7, r7, #0xc
	str.w r2, [r3, #0x110]
	str.w ip, [r3, #0x124]
	strd r7, r6, [r3, #0x150]
	movs r7, #9
	movt r7, #0x700
	str.w r5, [r3, #0x130]
	str.w r7, [r3, #0x168]
	mov.w r7, #0x28000000
	str.w r7, [r3, #0x174]
	mov.w r7, #0x4000000
	strd lr, r7, [r3, #0x19c]
	mov.w lr, #0xc
	movt lr, #0x700
	strd r7, r6, [r3, #0x178]
	str.w r2, [r3, #0x138]
	str.w r0, [r3, #0x158]
	str.w r2, [r3, #0x160]
	str.w r0, [r3, #0x180]
	str.w r2, [r3, #0x188]
	str.w r2, [r3, #0x1b0]
	str.w lr, [r3, #0x1b8]
	movw lr, #0x3eff
	str.w r8, [r3, #0x230]
	mov.w r8, #0x24000000
	strd r8, r7, [r3, #0x23c]
	movw r8, #0x8a1
	lsl.w r8, r8, #9
	lsl.w lr, lr, #0xc
	str.w r7, [r3, #0x1c4]
	strd r8, r5, [r3, #0x244]
	mov.w r8, #0x11
	movt r8, #0x700
	strd lr, r1, [r3, #0x1c8]
	str.w r8, [r3, #0x258]
	mov.w r8, #0x2c000000
	strd r8, r7, [r3, #0x264]
	mov.w lr, #0xe
	movw r7, #0x921
	movt lr, #0x700
	lsls r7, r7, #9
	str.w lr, [r3, #0x1e0]
	mov.w lr, #-0x80000000
	str.w r2, [r3, #0x228]
	strd r7, r0, [r3, #0x26c]
	movs r7, #0x12
	strd lr, lr, [r3, #0x1ec]
	mov.w lr, #0xf
	movt lr, #0x700
	movt r7, #0x700
	str.w r7, [r3, #0x280]
	mov.w r8, #0x13
	str.w lr, [r3, #0x208]
	mov.w r7, #0x38000000
	mov.w lr, #0x60000000
	str.w r7, [r3, #0x28c]
	str.w lr, [r3, #0x214]
	mov.w r7, #0x800000
	mov.w lr, #0x8000000
	movt r8, #0x700
	str.w r2, [r3, #0x250]
	str.w r2, [r3, #0x278]
	strd r1, r5, [r3, #0x1f4]
	strd lr, r1, [r3, #0x218]
	strd r7, r1, [r3, #0x290]
	str.w r0, [r3, #0x1d0]
	str.w r2, [r3, #0x1d8]
	str.w r2, [r3, #0x200]
	str.w r5, [r3, #0x220]
	str.w r5, [r3, #0x298]
	str.w r8, [r3, #0x2a8]
	mov.w r8, #0x3c000000
	strd r8, r7, [r3, #0x2b4]
	movs r7, #0x14
	strd r1, r5, [r3, #0x2bc]
	movt r7, #0x700
	strd r1, r5, [r3, #0x2e4]
	movs r1, #0x15
	movt r1, #0x700
	str.w r7, [r3, #0x2d0]
	str.w r1, [r3, #0x2f8]
	mov.w r7, #0x68000000
	mov.w r1, #0x2000
	str.w r0, [r3, #0x310]
	str.w r2, [r3, #0x2a0]
	str.w r2, [r3, #0x2c8]
	str.w r2, [r3, #0x2f0]
	str.w lr, [r3, #0x304]
	str.w r2, [r3, #0x318]
	strd r7, ip, [r3, #0x2dc]
	strd r1, r6, [r3, #0x308]
	add.w r3, r3, #0x320
	ldr r2, [r4, #0x4c]
	subs r2, r3, r2
	ldr r3, SGXRegisterDeviceLiteral314
	asrs r2, r0
	mov r0, r5
	muls r2, r3, r2
	str r2, [r4, #0x48]
	pop.w {r4, r5, r6, r7, r8, pc}
.LSGXRegisterDevice2f2:
	movs r0, #1
	pop.w {r4, r5, r6, r7, r8, pc}
.size SGXRegisterDevice, .-SGXRegisterDevice
.align 2
SGXRegisterDeviceLiteral2f8:
	.word SgxDeviceInitialise
SGXRegisterDeviceLiteral2fc:
	.word DevDeInitSGX
SGXRegisterDeviceLiteral300:
	.word SgxVerifyFirmware
SGXRegisterDeviceLiteral304:
	.word SgxDeviceInterrupt
SGXRegisterDeviceLiteral308:
	.word SgxDeviceCallback34
SGXRegisterDeviceLiteral30c:
	.word SgxDeviceCallback38
SGXRegisterDeviceLiteral310:
	.word SgxDeviceCallback40
SGXRegisterDeviceLiteral314:
	.word 0xcccccccd
.align 2
.global SgxDeviceInitialise
.type SgxDeviceInitialise, %function
.thumb_func
SgxDeviceInitialise:
	push.w {r0, r1, r2, r3, r4, r5, r6, r7, r8, lr}
	mov r6, r0
	ldr r7, SgxDeviceInitialiseLiteral94
	mov.w r1, #0x860
	add r2, sp, #4
	ldr r5, [r0, #0x4c]
	movs r0, #2
	ldr r3, [r7]
	mov r8, r7
	str r3, [sp, #0xc]
	movs r3, #0
	bl GpuAllocFlags
	mov r4, r0
	cbz r0, .LSgxDeviceInitialise26
.LSgxDeviceInitialise22:
	movs r0, #1
	b .LSgxDeviceInitialise80
.LSgxDeviceInitialise26:
	ldr r0, [sp, #4]
	mov r1, r4
	mov.w r2, #0x860
	bl OSMemSet
	ldr r3, [sp, #4]
	movs r2, #7
	mov r0, r6
	add r1, sp, #8
	str r2, [r3]
	mov r2, r4
	ldr r3, [sp, #4]
	str r4, [r3, #4]
	str r3, [r6, #0x54]
	str r5, [r3, #0x34]
	mov r3, r4
	bl BM_CreateContext
	mov r7, r0
	cmp r0, #0
	beq .LSgxDeviceInitialise22
	ldrd r3, r2, [sp, #4]
	str r2, [r3, #0x30]
.LSgxDeviceInitialise58:
	ldr r3, [r6, #0x48]
	cmp r4, r3
	bhs .LSgxDeviceInitialise7a
	ldr r3, [r5, #0x18]
	subs r3, #1
	cmp r3, #2
	bhi .LSgxDeviceInitialise74
	ldr r3, [r5, #0x10]
	cbz r3, .LSgxDeviceInitialise74
	mov r0, r7
	mov r1, r5
	bl BM_CreateHeap
	str r0, [r5, #0x1c]
.LSgxDeviceInitialise74:
	adds r4, #1
	adds r5, #0x28
	b .LSgxDeviceInitialise58
.LSgxDeviceInitialise7a:
	ldr r0, [sp, #4]
	bl MMU_BIFResetPDAlloc
.LSgxDeviceInitialise80:
	ldr r2, [sp, #0xc]
	ldr.w r3, [r8]
	cmp r2, r3
	beq .LSgxDeviceInitialise8e
	bl __stack_chk_fail
.LSgxDeviceInitialise8e:
	add sp, #0x10
	pop.w {r4, r5, r6, r7, r8, pc}
.size SgxDeviceInitialise, .-SgxDeviceInitialise
.align 2
SgxDeviceInitialiseLiteral94:
	.word __stack_chk_guard
#if FW_DEBUG
.align 2
.global DevDeInitSGX
.type DevDeInitSGX, %function
.thumb_func
DevDeInitSGX:
	ldr r3, DevDeInitSGXLiteralcc
	push.w {r0, r1, r4, r5, r6, r7, r8, sb, sl, lr}
	mov sl, lr
	ldr r4, [r0, #0x54]
	mov r5, r0
	mov sb, r3
	ldr r2, [r3]
	str r2, [sp, #4]
	cbnz r4, .LDevDeInitSGX18
.LDevDeInitSGX14:
	movs r0, #0
	b .LDevDeInitSGXb8
.LDevDeInitSGX18:
	ldr.w r0, [r4, #0x7e0]
	cbz r0, .LDevDeInitSGX2a
	bl GpuTimerDestroy
	cmp r0, #0
	bne .LDevDeInitSGXb8
	str.w r0, [r4, #0x7e0]
.LDevDeInitSGX2a:
	mov r0, r4
	bl MmuBifResetPdFree
	ldr r2, [r4, #0x40]
	cbz r2, .LDevDeInitSGX3e
	movs r0, #1
	movs r1, #0x14
	movs r3, #0
	bl GpuFreeFlags
.LDevDeInitSGX3e:
	ldr r7, [r4, #0x34]
	mov.w r8, #0
	mov r6, r7
.LDevDeInitSGX46:
	ldr r3, [r5, #0x48]
	cmp r8, r3
	bhs .LDevDeInitSGX64
	ldr r3, [r6, #0x18]
	subs r3, #1
	cmp r3, #2
	bhi .LDevDeInitSGX5c
	ldr r0, [r6, #0x1c]
	cbz r0, .LDevDeInitSGX5c
	bl BM_DestroyHeap
.LDevDeInitSGX5c:
	add.w r8, r8, #1
	adds r6, #0x28
	b .LDevDeInitSGX46
.LDevDeInitSGX64:
	ldr r0, [r5, #0x50]
	movs r1, #0
	bl BM_DestroyContext
	cbnz r0, .LDevDeInitSGXb8
	ldr r0, [r5, #8]
	bl GpuRequestDestroy
	cbnz r0, .LDevDeInitSGXb8
	movs r0, #7
	mov r1, sp
	bl GpuLookupPlatformDevice
	cbnz r0, .LDevDeInitSGXb8
	ldr r3, [sp]
	ldr r3, [r3, #0xc]
	cbnz r3, .LDevDeInitSGX92
	ldr r3, [r4, #0x14]
	cbz r3, .LDevDeInitSGX92
	ldr r1, DevDeInitSGXLiterald0
	mov r2, sl
	bl SceDebugForDriver_35A35322
.LDevDeInitSGX92:
	mov r2, r4
	movs r0, #2
	mov.w r1, #0x860
	movs r3, #0
	movs r4, #0
	bl GpuFreeFlags
	str r4, [r5, #0x54]
	cmp r7, #0
	beq .LDevDeInitSGX14
	mov.w r1, #0x370
	mov r2, r7
	mov r3, r4
	movs r0, #1
	bl GpuFreeFlags
	b .LDevDeInitSGX14
.LDevDeInitSGXb8:
	ldr r2, [sp, #4]
	ldr.w r3, [sb]
	cmp r2, r3
	beq .LDevDeInitSGXc6
	bl __stack_chk_fail
.LDevDeInitSGXc6:
	add sp, #8
	pop.w {r4, r5, r6, r7, r8, sb, sl, pc}
.size DevDeInitSGX, .-DevDeInitSGX
.align 2
DevDeInitSGXLiteralcc:
	.word __stack_chk_guard
DevDeInitSGXLiterald0:
	.word sgxDebugInfo0
#else
.align 2
.global DevDeInitSGX
.type DevDeInitSGX, %function
.thumb_func
DevDeInitSGX:
	ldr r3, DevDeInitSGXLiteralb8
	push.w {r0, r1, r2, r4, r5, r6, r7, r8, sb, lr}
	mov r6, r0
	ldr r4, [r0, #0x54]
	mov sb, r3
	ldr r2, [r3]
	str r2, [sp, #4]
	cbnz r4, .LDevDeInitSGX16
.LDevDeInitSGX12:
	movs r0, #0
	b .LDevDeInitSGXa4
.LDevDeInitSGX16:
	ldr.w r0, [r4, #0x7e0]
	cbz r0, .LDevDeInitSGX28
	bl GpuTimerDestroy
	cmp r0, #0
	bne .LDevDeInitSGXa4
	str.w r0, [r4, #0x7e0]
.LDevDeInitSGX28:
	mov r0, r4
	bl MmuBifResetPdFree
	ldr r2, [r4, #0x40]
	cbz r2, .LDevDeInitSGX3c
	movs r0, #1
	movs r1, #0x14
	movs r3, #0
	bl GpuFreeFlags
.LDevDeInitSGX3c:
	ldr r7, [r4, #0x34]
	mov.w r8, #0
	mov r5, r7
.LDevDeInitSGX44:
	ldr r3, [r6, #0x48]
	cmp r8, r3
	bhs .LDevDeInitSGX62
	ldr r3, [r5, #0x18]
	subs r3, #1
	cmp r3, #2
	bhi .LDevDeInitSGX5a
	ldr r0, [r5, #0x1c]
	cbz r0, .LDevDeInitSGX5a
	bl BM_DestroyHeap
.LDevDeInitSGX5a:
	add.w r8, r8, #1
	adds r5, #0x28
	b .LDevDeInitSGX44
.LDevDeInitSGX62:
	ldr r0, [r6, #0x50]
	movs r1, #0
	bl BM_DestroyContext
	cbnz r0, .LDevDeInitSGXa4
	ldr r0, [r6, #8]
	bl GpuRequestDestroy
	cbnz r0, .LDevDeInitSGXa4
	movs r0, #7
	mov r1, sp
	bl GpuLookupPlatformDevice
	mov r5, r0
	cbnz r0, .LDevDeInitSGXa4
	movs r0, #2
	mov.w r1, #0x860
	mov r2, r4
	mov r3, r5
	bl GpuFreeFlags
	str r5, [r6, #0x54]
	cmp r7, #0
	beq .LDevDeInitSGX12
	mov.w r1, #0x370
	mov r2, r7
	mov r3, r5
	movs r0, #1
	bl GpuFreeFlags
	b .LDevDeInitSGX12
.LDevDeInitSGXa4:
	ldr r2, [sp, #4]
	ldr.w r3, [sb]
	cmp r2, r3
	beq .LDevDeInitSGXb2
	bl __stack_chk_fail
.LDevDeInitSGXb2:
	add sp, #0xc
	pop.w {r4, r5, r6, r7, r8, sb, pc}
.size DevDeInitSGX, .-DevDeInitSGX
.align 2
DevDeInitSGXLiteralb8:
	.word __stack_chk_guard
#endif
#if FW_DEBUG
.align 2
.global SgxVerifyFirmware
.type SgxVerifyFirmware, %function
.thumb_func
SgxVerifyFirmware:
	push {r3, r4, r5, r6, r7, lr}
	mov r1, r0
	ldr r3, [r0]
	cmp r3, #7
	beq .LSgxVerifyFirmware14
	ldr r0, SgxVerifyFirmwareLiteral19c
	movs r5, #3
	bl VBAppend
	b .LSgxVerifyFirmware198
.LSgxVerifyFirmware14:
	ldr r4, [r0, #0x54]
	ldr r2, SgxVerifyFirmwareLiteral1a0
	ldr.w r3, [r4, #0x98]
	cmp r3, r2
	beq .LSgxVerifyFirmware3e
	mov r4, r2
	eors r4, r3
	ands.w r1, r4, r3
	beq .LSgxVerifyFirmware30
	ldr r0, SgxVerifyFirmwareLiteral1a4
	bl VBAppend
.LSgxVerifyFirmware30:
	ldr r1, SgxVerifyFirmwareLiteral1a0
	ands r1, r4
	cmp r1, #0
	beq.w .LSgxVerifyFirmware188
	ldr r0, SgxVerifyFirmwareLiteral1a8
	b .LSgxVerifyFirmware184
.LSgxVerifyFirmware3e:
	ldr r7, [r4, #0x54]
	movs r2, #2
	ldr r3, [r7]
	str r2, [r3]
	movs r2, #0
	ldr r0, [r4, #0x54]
	bl SgxWaitFirmwareCommand
	mov r5, r0
	cbz r0, .LSgxVerifyFirmware5a
	ldr r0, SgxVerifyFirmwareLiteral1ac
	bl VBAppend
	b .LSgxVerifyFirmware198
.LSgxVerifyFirmware5a:
	ldr r6, [r7]
	ldr r2, SgxVerifyFirmwareLiteral1b0
	ldr r3, [r6, #0xc]
	cmp r3, r2
	bne .LSgxVerifyFirmware6e
	ldr r3, [r6, #0x10]
	movw r2, #0x13a5
	cmp r3, r2
	beq .LSgxVerifyFirmware7e
.LSgxVerifyFirmware6e:
	ldr r0, SgxVerifyFirmwareLiteral1b4
	movw r1, #0x13a5
	ldr r2, [r6, #0x10]
	movs r5, #0x19
	bl VBAppend
	b .LSgxVerifyFirmware198
.LSgxVerifyFirmware7e:
	ldr r2, [r6, #0x18]
	cbz r2, .LSgxVerifyFirmware9e
	ldr r1, [r6, #4]
	movw r3, #0x101
	cmp.w r1, r3, lsl #8
	bne .LSgxVerifyFirmware18c
	add.w r3, r3, #0x10000
	cmp r2, r3
	bne .LSgxVerifyFirmware18c
	ldr r0, SgxVerifyFirmwareLiteral1b8
	bl VBAppend
	b .LSgxVerifyFirmwarea6
.LSgxVerifyFirmware9e:
	ldr r0, SgxVerifyFirmwareLiteral1bc
	ldr r1, [r6, #4]
	bl VBAppend
.LSgxVerifyFirmwarea6:
	ldr r2, [r7]
	ldrd r3, r1, [r4, #0xd0]
	ldrd r7, r0, [r2, #0x58]
	cmp r0, r1
	bne .LSgxVerifyFirmwarebc
	subs r3, r7, r3
	it ne
	movne r3, #1
	b .LSgxVerifyFirmwarebe
.LSgxVerifyFirmwarebc:
	movs r3, #1
.LSgxVerifyFirmwarebe:
	ldr r0, [r2, #0x24]
	ldr.w r1, [r4, #0x9c]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x28]
	ldr.w r1, [r4, #0xa0]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x2c]
	ldr.w r1, [r4, #0xa4]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x30]
	ldr.w r1, [r4, #0xa8]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x34]
	ldr.w r1, [r4, #0xac]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x38]
	ldr.w r1, [r4, #0xb0]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x3c]
	ldr.w r1, [r4, #0xb4]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x40]
	ldr.w r1, [r4, #0xb8]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x44]
	ldr.w r1, [r4, #0xbc]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x48]
	ldr.w r1, [r4, #0xc0]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x4c]
	ldr.w r1, [r4, #0xc4]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r0, [r2, #0x50]
	ldr.w r1, [r4, #0xc8]
	cmp r0, r1
	it ne
	movne r3, #1
	ldr r1, [r2, #0x54]
	ldr.w r2, [r4, #0xcc]
	cmp r1, r2
	bne .LSgxVerifyFirmware15c
	cmp r3, #1
	bne .LSgxVerifyFirmware164
.LSgxVerifyFirmware15c:
	ldr r0, SgxVerifyFirmwareLiteral1c0
	bl VBAppend
	b .LSgxVerifyFirmware188
.LSgxVerifyFirmware164:
	ldr r6, [r6, #0x1c]
	ldr r3, SgxVerifyFirmwareLiteral1a0
	cmp r6, r3
	beq .LSgxVerifyFirmware198
	mov r4, r3
	mov r1, r3
	eors r4, r6
	ands r1, r4
	cbz r1, .LSgxVerifyFirmware17c
	ldr r0, SgxVerifyFirmwareLiteral1c4
	bl VBAppend
.LSgxVerifyFirmware17c:
	ands.w r1, r4, r6
	beq .LSgxVerifyFirmware188
	ldr r0, SgxVerifyFirmwareLiteral1c8
.LSgxVerifyFirmware184:
	bl VBAppend
.LSgxVerifyFirmware188:
	movs r5, #0x1a
	b .LSgxVerifyFirmware198
.LSgxVerifyFirmware18c:
	cmp r1, r2
	beq .LSgxVerifyFirmwarea6
	ldr r0, SgxVerifyFirmwareLiteral1cc
	bl VBAppend
	b .LSgxVerifyFirmware188
.LSgxVerifyFirmware198:
	mov r0, r5
	pop {r3, r4, r5, r6, r7, pc}
.size SgxVerifyFirmware, .-SgxVerifyFirmware
.align 2
SgxVerifyFirmwareLiteral19c:
	.word sgxConstantString1
SgxVerifyFirmwareLiteral1a0:
	.word 0x30322518
SgxVerifyFirmwareLiteral1a4:
	.word sgxConstantString2
SgxVerifyFirmwareLiteral1a8:
	.word sgxConstantString3
SgxVerifyFirmwareLiteral1ac:
	.word sgxConstantString4
SgxVerifyFirmwareLiteral1b0:
	.word 0x10711
SgxVerifyFirmwareLiteral1b4:
	.word sgxConstantString5
SgxVerifyFirmwareLiteral1b8:
	.word sgxConstantString6
SgxVerifyFirmwareLiteral1bc:
	.word sgxConstantString7
SgxVerifyFirmwareLiteral1c0:
	.word sgxConstantString8
SgxVerifyFirmwareLiteral1c4:
	.word sgxConstantString9
SgxVerifyFirmwareLiteral1c8:
	.word sgxConstantString10
SgxVerifyFirmwareLiteral1cc:
	.word sgxConstantString11
#else
.align 2
.global SgxVerifyFirmware
.type SgxVerifyFirmware, %function
.thumb_func
SgxVerifyFirmware:
	push {r4, r5, r6, lr}
	mov r1, r0
	ldr r3, [r0]
	cmp r3, #7
	bne.w .LSgxVerifyFirmware120
	ldr r4, [r0, #0x54]
	ldr r2, SgxVerifyFirmwareLiteral134
	ldr.w r3, [r4, #0x98]
	cmp r3, r2
	beq .LSgxVerifyFirmware1c
.LSgxVerifyFirmware18:
	movs r0, #0x1a
	pop {r4, r5, r6, pc}
.LSgxVerifyFirmware1c:
	ldr r5, [r4, #0x54]
	movs r2, #2
	ldr r3, [r5]
	str r2, [r3]
	movs r2, #0
	ldr r0, [r4, #0x54]
	bl SgxWaitFirmwareCommand
	cmp r0, #0
	bne .LSgxVerifyFirmware130
	ldr r2, [r5]
	ldr r1, SgxVerifyFirmwareLiteral138
	ldr r3, [r2, #0xc]
	cmp r3, r1
	bne .LSgxVerifyFirmware124
	ldr r3, [r2, #0x10]
	movw r1, #0x13a5
	cmp r3, r1
	bne .LSgxVerifyFirmware124
	ldr r3, [r2, #0x18]
	cbz r3, .LSgxVerifyFirmware5c
	ldr r1, [r2, #4]
	movw r5, #0x101
	cmp.w r1, r5, lsl #8
	bne .LSgxVerifyFirmware128
	add.w r5, r5, #0x10000
	cmp r3, r5
	bne .LSgxVerifyFirmware128
.LSgxVerifyFirmware5c:
	ldr r5, [r2, #0x5c]
	ldr.w r1, [r4, #0xd4]
	ldr r6, [r2, #0x58]
	ldr.w r3, [r4, #0xd0]
	cmp r5, r1
	bne .LSgxVerifyFirmware74
	subs r3, r6, r3
	it ne
	movne r3, #1
	b .LSgxVerifyFirmware76
.LSgxVerifyFirmware74:
	movs r3, #1
.LSgxVerifyFirmware76:
	ldr.w r1, [r4, #0x9c]
	ldr r5, [r2, #0x24]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xa0]
	ldr r5, [r2, #0x28]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xa4]
	ldr r5, [r2, #0x2c]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xa8]
	ldr r5, [r2, #0x30]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xac]
	ldr r5, [r2, #0x34]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xb0]
	ldr r5, [r2, #0x38]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xb4]
	ldr r5, [r2, #0x3c]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xb8]
	ldr r5, [r2, #0x40]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xbc]
	ldr r5, [r2, #0x44]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xc0]
	ldr r5, [r2, #0x48]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xc4]
	ldr r5, [r2, #0x4c]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr.w r1, [r4, #0xc8]
	ldr r5, [r2, #0x50]
	ldr.w r4, [r4, #0xcc]
	cmp r5, r1
	it ne
	movne r3, #1
	ldr r1, [r2, #0x54]
	cmp r1, r4
	bne .LSgxVerifyFirmware18
	cmp r3, #1
	beq .LSgxVerifyFirmware18
	ldr r2, [r2, #0x1c]
	ldr r3, SgxVerifyFirmwareLiteral134
	cmp r2, r3
	bne.w .LSgxVerifyFirmware18
	pop {r4, r5, r6, pc}
.LSgxVerifyFirmware120:
	movs r0, #3
	pop {r4, r5, r6, pc}
.LSgxVerifyFirmware124:
	movs r0, #0x19
	pop {r4, r5, r6, pc}
.LSgxVerifyFirmware128:
	cmp r1, r3
	bne.w .LSgxVerifyFirmware18
	b .LSgxVerifyFirmware5c
.LSgxVerifyFirmware130:
	pop {r4, r5, r6, pc}
.size SgxVerifyFirmware, .-SgxVerifyFirmware
.align 2
SgxVerifyFirmwareLiteral134:
	.word 0x30322518
SgxVerifyFirmwareLiteral138:
	.word 0x10711
#endif
.align 2
.global SgxDeviceInterrupt
.type SgxDeviceInterrupt, %function
.thumb_func
SgxDeviceInterrupt:
	push {r4, r5, r6, lr}
	cbnz r0, .LSgxDeviceInterrupt8
.LSgxDeviceInterrupt4:
	movs r0, #0
	pop {r4, r5, r6, pc}
.LSgxDeviceInterrupt8:
	ldr r4, [r0, #0x54]
	mov.w r1, #0x12c
	ldr r0, [r4, #0x14]
	bl GpuReadRegister
	mov.w r1, #0x130
	mov r5, r0
	ldr r0, [r4, #0x14]
	bl GpuReadRegister
	mov.w r1, #0x118
	ands r5, r0
	ldr r0, [r4, #0x14]
	bl GpuReadRegister
	mov.w r1, #0x110
	mov r6, r0
	ldr r0, [r4, #0x14]
	bl GpuReadRegister
	ands r0, r6
	and r2, r5, #0x4000
	lsls r3, r0, #0x15
	and r5, r0, #0x800
	it mi
	orrmi r5, r5, #0x400
	cbnz r2, .LSgxDeviceInterrupt50
	cmp r5, #0
	beq .LSgxDeviceInterrupt4
.LSgxDeviceInterrupt50:
	ldr r0, [r4, #0x14]
	mov.w r1, #0x134
	orr r2, r2, #0x80000000
	bl GpuWriteRegister
	ldr r0, [r4, #0x14]
	mov.w r1, #0x114
	mov r2, r5
	bl GpuWriteRegister
	movs r0, #1
	pop {r4, r5, r6, pc}
.size SgxDeviceInterrupt, .-SgxDeviceInterrupt
.align 2
.align 2
.global SgxDeviceCallback34
.type SgxDeviceCallback34, %function
.thumb_func
SgxDeviceCallback34:
	push {r4, lr}
	mov r4, r0
	ldr r3, [r0, #0x54]
	ldr.w r3, [r3, #0x7f0]
	ldr r2, [r3, #0x20]
	lsls r2, r2, #0x1f
	bpl .LSgxDeviceCallback3420
	ldr r3, [r3, #0x24]
	lsls r3, r3, #0x1f
	bmi .LSgxDeviceCallback3420
	bl GpuGetLockupEvent
	movs r1, #1
	bl sceKernelSetEventFlag
.LSgxDeviceCallback3420:
	mov r0, r4
	mvn r1, #2
	bl GpuRuntimePendingDrain
	pop {r4, pc}
.size SgxDeviceCallback34, .-SgxDeviceCallback34
.align 2
.align 2
.global SgxDeviceCallback38
.type SgxDeviceCallback38, %function
.thumb_func
SgxDeviceCallback38:
	push {r3, lr}
	bl GpuRuntimeSuspendCommand
	pop {r3, pc}
.size SgxDeviceCallback38, .-SgxDeviceCallback38
.align 2
.align 2
.global SgxDeviceCallback40
.type SgxDeviceCallback40, %function
.thumb_func
SgxDeviceCallback40:
	ldr r3, [r0, #0x54]
	ldr.w r2, [r3, #0x94]
	orr r2, r2, #4
	str.w r2, [r3, #0x94]
	bx lr
.size SgxDeviceCallback40, .-SgxDeviceCallback40
.align 2
#if FW_DEBUG
.align 2
.global BM_CreateContext
.type BM_CreateContext, %function
.thumb_func
BM_CreateContext:
	push.w {r4, r5, r6, r7, r8, sb, sl, lr}
	mov r4, r3
	ldr r6, BM_CreateContextLiteral188
	mov r5, r0
	clz r0, r2
	sub sp, #0x18
	mov sb, r1
	lsrs r0, r0, #5
	ldr r3, [r6]
	mov r2, lr
	ldr r1, BM_CreateContextLiteral18c
	mov r8, r6
	str r3, [sp, #0x14]
	bl SceDebugForDriver_35A35322
	cbz r4, .LBM_CreateContext28
	movs r3, #0
	str r3, [r4]
.LBM_CreateContext28:
	cbz r5, .LBM_CreateContext2e
	ldr r3, [r5, #0x54]
	cbnz r3, .LBM_CreateContext40
.LBM_CreateContext2e:
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral194
	movs r0, #1
	str r3, [sp]
	ldr r3, BM_CreateContextLiteral198
	bl SceDebugForDriver_D9703808
	b .LBM_CreateContext106
.LBM_CreateContext40:
	mov r0, r5
	bl GpuSetLockupDevice
	bl sceGpuInitMemoryContexts
	cmp r0, #0
	bge .LBM_CreateContext5e
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral19c
	strd r3, r0, [sp]
	movs r0, #1
	ldr r3, BM_CreateContextLiteral1a0
	b .LBM_CreateContext102
.LBM_CreateContext5e:
	add r0, sp, #0xc
	bl GetKernelMemoryContext
	cmp r0, #0
	bge .LBM_CreateContext78
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral1a4
	strd r3, r0, [sp]
	movs r0, #1
	ldr r3, BM_CreateContextLiteral1a8
	b .LBM_CreateContext102
.LBM_CreateContext78:
	ldr r0, [sp, #0xc]
	movs r1, #5
	movt r1, #1
	adds r0, #0x10
	bl sceGpuMmuAllocatePageTablePool
	cmp r0, #0
	bge .LBM_CreateContext9a
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral1ac
	strd r3, r0, [sp]
	movs r0, #1
	ldr r3, BM_CreateContextLiteral1b0
	b .LBM_CreateContext102
.LBM_CreateContext9a:
	ldr r0, BM_CreateContextLiteral1b4
	mov.w r2, #0x80000
	ldr r1, BM_CreateContextLiteral1b8
	movs r3, #0
	bl SceSysmemForDriver_C94850C9
	subs.w sl, r0, #0
	bge .LBM_CreateContextbe
	ldr r3, BM_CreateContextLiteral190
	movs r0, #1
	movs r1, #0
	ldr r2, BM_CreateContextLiteral1bc
	stm.w sp, {r3, sl}
	ldr r3, BM_CreateContextLiteral1c0
	b .LBM_CreateContext102
.LBM_CreateContextbe:
	add r7, sp, #0x18
	movs r6, #1
	str r6, [r7, #-0x8]!
	bl GetKernelMemBlockHeap
	mov r1, sl
	mov r2, r7
	movs r3, #0
	bl sceGpuMemBlockAddBlock
	cmp r0, #0
	bge .LBM_CreateContexte8
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral1c4
	strd r3, r0, [sp]
	mov r0, r6
	ldr r3, BM_CreateContextLiteral1c8
	b .LBM_CreateContext102
.LBM_CreateContexte8:
	ldr r0, [sp, #0xc]
	adds r0, #4
	bl AllocateGpuPageDirectory
	cmp r0, #0
	bge .LBM_CreateContext10a
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral1cc
	strd r3, r0, [sp]
	mov r0, r6
	ldr r3, BM_CreateContextLiteral1d0
.LBM_CreateContext102:
	bl SceDebugForDriver_D9703808
.LBM_CreateContext106:
	movs r0, #0
	b .LBM_CreateContext174
.LBM_CreateContext10a:
	ldr r0, BM_CreateContextLiteral1d4
	movs r1, #0x10
	bl SceProcessmgrForDriver_3801D7D6
	subs r3, r0, #0
	bge .LBM_CreateContext12a
	ldr r2, BM_CreateContextLiteral190
	mov r0, r6
	movs r1, #0
	stm.w sp, {r2, r3}
	ldr r2, BM_CreateContextLiteral1d8
	ldr r3, BM_CreateContextLiteral1dc
	bl SceDebugForDriver_D9703808
	b .LBM_CreateContext15e
.LBM_CreateContext12a:
	ldr r2, [sp, #0xc]
	add.w r2, r2, #0x1020
	str r3, [r2, #4]
	bl SceProcessmgrForDriver_EE694840
	cbnz r0, .LBM_CreateContext14a
	ldr r3, BM_CreateContextLiteral190
	movs r1, #0
	ldr r2, BM_CreateContextLiteral1e0
	movs r0, #1
	str r3, [sp]
	ldr r3, BM_CreateContextLiteral1e4
	bl SceDebugForDriver_D9703808
	b .LBM_CreateContext15e
.LBM_CreateContext14a:
	ldr r3, [sp, #0xc]
	ldr r3, [r3, #4]
	str r3, [r0]
	ldr r3, [sp, #0xc]
	ldr r1, [r3, #8]
	str r1, [r0, #4]
	ldr r3, [r3, #0xc]
	str r3, [r0, #8]
	movs r3, #0
	str r3, [r0, #0xc]
.LBM_CreateContext15e:
	ldr r0, [sp, #0xc]
	str r0, [r5, #0x50]
	cmp.w sb, #0
	beq .LBM_CreateContext16e
	ldr r3, [r0, #0xc]
	str.w r3, [sb]
.LBM_CreateContext16e:
	cbz r4, .LBM_CreateContext174
	movs r3, #1
	str r3, [r4]
.LBM_CreateContext174:
	ldr r2, [sp, #0x14]
	ldr.w r3, [r8]
	cmp r2, r3
	beq .LBM_CreateContext182
	bl __stack_chk_fail
.LBM_CreateContext182:
	add sp, #0x18
	pop.w {r4, r5, r6, r7, r8, sb, sl, pc}
.size BM_CreateContext, .-BM_CreateContext
.align 2
BM_CreateContextLiteral188:
	.word __stack_chk_guard
BM_CreateContextLiteral18c:
	.word sgxDebugInfo1
BM_CreateContextLiteral190:
	.word sgxConstantString13
BM_CreateContextLiteral194:
	.word sgxDebugInfo2
BM_CreateContextLiteral198:
	.word sgxConstantString14
BM_CreateContextLiteral19c:
	.word sgxDebugInfo3
BM_CreateContextLiteral1a0:
	.word sgxConstantString15
BM_CreateContextLiteral1a4:
	.word sgxDebugInfo4
BM_CreateContextLiteral1a8:
	.word sgxConstantString16
BM_CreateContextLiteral1ac:
	.word sgxDebugInfo5
BM_CreateContextLiteral1b0:
	.word sgxConstantString17
BM_CreateContextLiteral1b4:
	.word sgxConstantString18
BM_CreateContextLiteral1b8:
	.word 0x10208006
BM_CreateContextLiteral1bc:
	.word sgxDebugInfo6
BM_CreateContextLiteral1c0:
	.word sgxConstantString19
BM_CreateContextLiteral1c4:
	.word sgxDebugInfo7
BM_CreateContextLiteral1c8:
	.word sgxConstantString20
BM_CreateContextLiteral1cc:
	.word sgxDebugInfo8
BM_CreateContextLiteral1d0:
	.word sgxConstantString21
BM_CreateContextLiteral1d4:
	.word sgxConstantString22
BM_CreateContextLiteral1d8:
	.word sgxDebugInfo9
BM_CreateContextLiteral1dc:
	.word sgxConstantString23
BM_CreateContextLiteral1e0:
	.word sgxDebugInfo10
BM_CreateContextLiteral1e4:
	.word sgxConstantString24
#else
.align 2
.global BM_CreateContext
.type BM_CreateContext, %function
.thumb_func
BM_CreateContext:
	ldr r2, BM_CreateContextLiteraldc
	push.w {r4, r5, r6, r7, r8, sb, lr}
	mov r7, r1
	sub sp, #0x14
	mov r5, r0
	mov r4, r3
	mov r8, r2
	ldr r1, [r2]
	str r1, [sp, #0xc]
	cbz r3, .LBM_CreateContext1a
	movs r3, #0
	str r3, [r4]
.LBM_CreateContext1a:
	cbnz r5, .LBM_CreateContext20
.LBM_CreateContext1c:
	movs r0, #0
	b .LBM_CreateContextc8
.LBM_CreateContext20:
	ldr r3, [r5, #0x54]
	cmp r3, #0
	beq .LBM_CreateContext1c
	mov r0, r5
	bl GpuSetLockupDevice
	bl sceGpuInitMemoryContexts
	cmp r0, #0
	blt .LBM_CreateContext1c
	add r0, sp, #4
	bl GetKernelMemoryContext
	cmp r0, #0
	blt .LBM_CreateContext1c
	ldr r0, [sp, #4]
	movs r1, #5
	movt r1, #1
	adds r0, #0x10
	bl sceGpuMmuAllocatePageTablePool
	cmp r0, #0
	blt .LBM_CreateContext1c
	ldr r0, BM_CreateContextLiterale0
	mov.w r2, #0x80000
	ldr r1, BM_CreateContextLiterale4
	movs r3, #0
	bl SceSysmemForDriver_C94850C9
	subs.w sb, r0, #0
	blt .LBM_CreateContext1c
	movs r3, #1
	add r6, sp, #0x10
	str r3, [r6, #-0x8]!
	bl GetKernelMemBlockHeap
	mov r1, sb
	mov r2, r6
	movs r3, #0
	bl sceGpuMemBlockAddBlock
	cmp r0, #0
	blt .LBM_CreateContext1c
	ldr r0, [sp, #4]
	adds r0, #4
	bl AllocateGpuPageDirectory
	cmp r0, #0
	blt .LBM_CreateContext1c
	ldr r0, BM_CreateContextLiterale8
	movs r1, #0x10
	bl SceProcessmgrForDriver_3801D7D6
	subs r2, r0, #0
	blt .LBM_CreateContextb8
	ldr r3, [sp, #4]
	add.w r3, r3, #0x1020
	str r2, [r3, #4]
	bl SceProcessmgrForDriver_EE694840
	cbz r0, .LBM_CreateContextb8
	ldr r3, [sp, #4]
	ldr r3, [r3, #4]
	str r3, [r0]
	ldr r3, [sp, #4]
	ldr r1, [r3, #8]
	str r1, [r0, #4]
	ldr r3, [r3, #0xc]
	str r3, [r0, #8]
	movs r3, #0
	str r3, [r0, #0xc]
.LBM_CreateContextb8:
	ldr r0, [sp, #4]
	str r0, [r5, #0x50]
	cbz r7, .LBM_CreateContextc2
	ldr r3, [r0, #0xc]
	str r3, [r7]
.LBM_CreateContextc2:
	cbz r4, .LBM_CreateContextc8
	movs r3, #1
	str r3, [r4]
.LBM_CreateContextc8:
	ldr r2, [sp, #0xc]
	ldr.w r3, [r8]
	cmp r2, r3
	beq .LBM_CreateContextd6
	bl __stack_chk_fail
.LBM_CreateContextd6:
	add sp, #0x14
	pop.w {r4, r5, r6, r7, r8, sb, pc}
.size BM_CreateContext, .-BM_CreateContext
.align 2
BM_CreateContextLiteraldc:
	.word __stack_chk_guard
BM_CreateContextLiterale0:
	.word sgxConstantString18
BM_CreateContextLiterale4:
	.word 0x10208006
BM_CreateContextLiterale8:
	.word sgxConstantString22
#endif
#if FW_DEBUG
.align 2
.global BM_CreateHeap
.type BM_CreateHeap, %function
.thumb_func
BM_CreateHeap:
	push {r4, r5, r6, lr}
	sub sp, #0x18
	ldr r5, BM_CreateHeapLiteral78
	mov r4, r1
	mov r6, r0
	ldr r2, [r4]
	ldr r3, [r5]
	ldrd r0, r1, [r4, #0xc]
	str r3, [sp, #0x14]
	add r3, sp, #0x10
	str r3, [sp]
	ldr r3, [r4, #0x14]
	bl CreateGpuHeap
	cmp r0, #0
	bge .LBM_CreateHeap38
	ldr r3, BM_CreateHeapLiteral7c
	movs r1, #0
	ldr r2, BM_CreateHeapLiteral80
	str r3, [sp]
	ldr r3, [r4]
	strd r3, r0, [sp, #4]
	movs r0, #1
	ldr r3, BM_CreateHeapLiteral84
	bl SceDebugForDriver_D9703808
.LBM_CreateHeap38:
	ldr r3, [r4, #0x18]
	cmp r3, #3
	bne .LBM_CreateHeap44
	ldr r3, [sp, #0x10]
	movs r2, #1
	str r2, [r3, #0x10]
.LBM_CreateHeap44:
	mov r0, r6
	ldr r1, [sp, #0x10]
	bl AddGpuHeapToMemoryContext
	cmp r0, #0
	bge .LBM_CreateHeap66
	ldr r3, BM_CreateHeapLiteral7c
	movs r1, #0
	ldr r2, BM_CreateHeapLiteral88
	str r3, [sp]
	ldr r3, [r4]
	strd r3, r0, [sp, #4]
	movs r0, #1
	ldr r3, BM_CreateHeapLiteral8c
	bl SceDebugForDriver_D9703808
.LBM_CreateHeap66:
	ldr r2, [sp, #0x14]
	ldr r3, [r5]
	ldr r0, [sp, #0x10]
	cmp r2, r3
	beq .LBM_CreateHeap74
	bl __stack_chk_fail
.LBM_CreateHeap74:
	add sp, #0x18
	pop {r4, r5, r6, pc}
.size BM_CreateHeap, .-BM_CreateHeap
.align 2
BM_CreateHeapLiteral78:
	.word __stack_chk_guard
BM_CreateHeapLiteral7c:
	.word sgxConstantString25
BM_CreateHeapLiteral80:
	.word sgxDebugInfo11
BM_CreateHeapLiteral84:
	.word sgxConstantString26
BM_CreateHeapLiteral88:
	.word sgxDebugInfo12
BM_CreateHeapLiteral8c:
	.word sgxConstantString27
#else
.align 2
.global BM_CreateHeap
.type BM_CreateHeap, %function
.thumb_func
BM_CreateHeap:
	push {r0, r1, r2, r3, r4, r5, r6, lr}
	mov r4, r1
	ldr r5, BM_CreateHeapLiteral44
	mov r6, r0
	ldr r2, [r4]
	ldr r3, [r5]
	ldrd r0, r1, [r4, #0xc]
	str r3, [sp, #0xc]
	add r3, sp, #8
	str r3, [sp]
	ldr r3, [r4, #0x14]
	bl CreateGpuHeap
	ldr r3, [r4, #0x18]
	cmp r3, #3
	bne .LBM_CreateHeap28
	ldr r3, [sp, #8]
	movs r2, #1
	str r2, [r3, #0x10]
.LBM_CreateHeap28:
	mov r0, r6
	ldr r1, [sp, #8]
	bl AddGpuHeapToMemoryContext
	ldr r2, [sp, #0xc]
	ldr r3, [r5]
	ldr r0, [sp, #8]
	cmp r2, r3
	beq .LBM_CreateHeap3e
	bl __stack_chk_fail
.LBM_CreateHeap3e:
	add sp, #0x10
	pop {r4, r5, r6, pc}
.size BM_CreateHeap, .-BM_CreateHeap
.align 2
BM_CreateHeapLiteral44:
	.word __stack_chk_guard
#endif
#if FW_DEBUG
.align 2
.global MMU_BIFResetPDAlloc
.type MMU_BIFResetPDAlloc, %function
.thumb_func
MMU_BIFResetPDAlloc:
	push {r4, r5, r6, lr}
	sub sp, #0x18
	ldr r6, MMU_BIFResetPDAllocLiteralf8
	mov r4, r0
	mov.w r2, #0x3000
	ldr r0, MMU_BIFResetPDAllocLiteralfc
	ldr r3, [r6]
	ldr r1, MMU_BIFResetPDAllocLiteral100
	str r3, [sp, #0x14]
	movs r3, #0
	bl SceSysmemForDriver_C94850C9
	subs r5, r0, #0
	blt .LMMU_BIFResetPDAlloce6
	add r1, sp, #0x10
	bl SceSysmemForDriver_A841EDDA
	cmp r0, #0
	bge .LMMU_BIFResetPDAlloc3a
	ldr r3, MMU_BIFResetPDAllocLiteral104
	movs r1, #0
	str r0, [sp, #8]
	movs r0, #1
	ldr r2, MMU_BIFResetPDAllocLiteral108
	stm.w sp, {r3, r5}
	ldr r3, MMU_BIFResetPDAllocLiteral10c
	b .LMMU_BIFResetPDAlloc9e
.LMMU_BIFResetPDAlloc3a:
	ldr r0, [sp, #0x10]
	addw r1, r4, #0x7cc
	bl SceSysmemForDriver_8D160E65
	cmp r0, #0
	ldr r3, [sp, #0x10]
	bge .LMMU_BIFResetPDAlloc5a
	ldr r2, MMU_BIFResetPDAllocLiteral104
	movs r1, #0
	str r0, [sp, #8]
	movs r0, #1
	stm.w sp, {r2, r3}
	ldr r2, MMU_BIFResetPDAllocLiteral110
	b .LMMU_BIFResetPDAlloc9c
.LMMU_BIFResetPDAlloc5a:
	add.w r0, r3, #0x1000
	add.w r1, r4, #0x7d0
	bl SceSysmemForDriver_8D160E65
	cmp r0, #0
	ldr r3, [sp, #0x10]
	bge .LMMU_BIFResetPDAlloc7c
	ldr r2, MMU_BIFResetPDAllocLiteral104
	movs r1, #0
	str r0, [sp, #8]
	movs r0, #1
	stm.w sp, {r2, r3}
	ldr r2, MMU_BIFResetPDAllocLiteral114
	b .LMMU_BIFResetPDAlloc9c
.LMMU_BIFResetPDAlloc7c:
	add.w r0, r3, #0x2000
	addw r1, r4, #0x7d4
	bl SceSysmemForDriver_8D160E65
	subs r3, r0, #0
	ldr r0, [sp, #0x10]
	bge .LMMU_BIFResetPDAllocaa
	ldr r2, MMU_BIFResetPDAllocLiteral104
	movs r1, #0
	str r3, [sp, #8]
	strd r2, r0, [sp]
	movs r0, #1
	ldr r2, MMU_BIFResetPDAllocLiteral118
.LMMU_BIFResetPDAlloc9c:
	ldr r3, MMU_BIFResetPDAllocLiteral11c
.LMMU_BIFResetPDAlloc9e:
	bl SceDebugForDriver_D9703808
	mov r0, r5
	bl SceSysmemForDriver_009E1C61
	b .LMMU_BIFResetPDAlloce6
.LMMU_BIFResetPDAllocaa:
	add.w r3, r0, #0x1000
	str.w r0, [r4, #0x7d8]
	str.w r3, [r4, #0x7dc]
	movs r1, #0
	mov.w r2, #0x1000
	str.w r5, [r4, #0x7c8]
	bl OSMemSet
	ldr.w r0, [r4, #0x7dc]
	movs r1, #0
	mov.w r2, #0x1000
	bl OSMemSet
	ldr r0, [sp, #0x10]
	movs r1, #0xdb
	mov.w r2, #0x1000
	add.w r0, r0, #0x2000
	bl OSMemSet
	movs r0, #0
	b .LMMU_BIFResetPDAlloce8
.LMMU_BIFResetPDAlloce6:
	movs r0, #1
.LMMU_BIFResetPDAlloce8:
	ldr r2, [sp, #0x14]
	ldr r3, [r6]
	cmp r2, r3
	beq .LMMU_BIFResetPDAllocf4
	bl __stack_chk_fail
.LMMU_BIFResetPDAllocf4:
	add sp, #0x18
	pop {r4, r5, r6, pc}
.size MMU_BIFResetPDAlloc, .-MMU_BIFResetPDAlloc
.align 2
MMU_BIFResetPDAllocLiteralf8:
	.word __stack_chk_guard
MMU_BIFResetPDAllocLiteralfc:
	.word sgxConstantString28
MMU_BIFResetPDAllocLiteral100:
	.word 0x10208006
MMU_BIFResetPDAllocLiteral104:
	.word sgxConstantString29
MMU_BIFResetPDAllocLiteral108:
	.word sgxDebugInfo13
MMU_BIFResetPDAllocLiteral10c:
	.word sgxConstantString31
MMU_BIFResetPDAllocLiteral110:
	.word sgxDebugInfo14
MMU_BIFResetPDAllocLiteral114:
	.word sgxDebugInfo15
MMU_BIFResetPDAllocLiteral118:
	.word sgxDebugInfo16
MMU_BIFResetPDAllocLiteral11c:
	.word sgxConstantString32
#else
.align 2
.global MMU_BIFResetPDAlloc
.type MMU_BIFResetPDAlloc, %function
.thumb_func
MMU_BIFResetPDAlloc:
	push {r0, r1, r4, r5, r6, lr}
	mov.w r2, #0x3000
	ldr r6, MMU_BIFResetPDAllocLiteralb0
	mov r4, r0
	ldr r1, MMU_BIFResetPDAllocLiteralb4
	ldr r3, [r6]
	ldr r0, MMU_BIFResetPDAllocLiteralb8
	str r3, [sp, #4]
	movs r3, #0
	bl SceSysmemForDriver_C94850C9
	subs r5, r0, #0
	blt .LMMU_BIFResetPDAlloc9e
	mov r1, sp
	bl SceSysmemForDriver_A841EDDA
	cmp r0, #0
	blt .LMMU_BIFResetPDAlloc58
	ldr r0, [sp]
	addw r1, r4, #0x7cc
	bl SceSysmemForDriver_8D160E65
	cmp r0, #0
	blt .LMMU_BIFResetPDAlloc58
	ldr r0, [sp]
	add.w r1, r4, #0x7d0
	add.w r0, r0, #0x1000
	bl SceSysmemForDriver_8D160E65
	cmp r0, #0
	blt .LMMU_BIFResetPDAlloc58
	ldr r0, [sp]
	addw r1, r4, #0x7d4
	add.w r0, r0, #0x2000
	bl SceSysmemForDriver_8D160E65
	cmp r0, #0
	bge .LMMU_BIFResetPDAlloc60
.LMMU_BIFResetPDAlloc58:
	mov r0, r5
	bl SceSysmemForDriver_009E1C61
	b .LMMU_BIFResetPDAlloc9e
.LMMU_BIFResetPDAlloc60:
	ldr r0, [sp]
	movs r1, #0
	mov.w r2, #0x1000
	str.w r5, [r4, #0x7c8]
	add.w r3, r0, #0x1000
	str.w r0, [r4, #0x7d8]
	str.w r3, [r4, #0x7dc]
	bl OSMemSet
	ldr.w r0, [r4, #0x7dc]
	movs r1, #0
	mov.w r2, #0x1000
	bl OSMemSet
	ldr r0, [sp]
	movs r1, #0xdb
	mov.w r2, #0x1000
	add.w r0, r0, #0x2000
	bl OSMemSet
	movs r0, #0
	b .LMMU_BIFResetPDAlloca0
.LMMU_BIFResetPDAlloc9e:
	movs r0, #1
.LMMU_BIFResetPDAlloca0:
	ldr r2, [sp, #4]
	ldr r3, [r6]
	cmp r2, r3
	beq .LMMU_BIFResetPDAllocac
	bl __stack_chk_fail
.LMMU_BIFResetPDAllocac:
	add sp, #8
	pop {r4, r5, r6, pc}
.size MMU_BIFResetPDAlloc, .-MMU_BIFResetPDAlloc
.align 2
MMU_BIFResetPDAllocLiteralb0:
	.word __stack_chk_guard
MMU_BIFResetPDAllocLiteralb4:
	.word 0x10208006
MMU_BIFResetPDAllocLiteralb8:
	.word sgxConstantString28
#endif
.align 2
.global GpuTimerDestroy
.type GpuTimerDestroy, %function
.thumb_func
GpuTimerDestroy:
	push {r4, lr}
	mov r4, r0
	ldr r0, [r0, #8]
	bl SceSystimerForDriver_8358611F
	mov r0, r4
	bl FreeProcessGpuMemory
	movs r0, #0
	pop {r4, pc}
.size GpuTimerDestroy, .-GpuTimerDestroy
.align 2
.align 2
.global MmuBifResetPdFree
.type MmuBifResetPdFree, %function
.thumb_func
MmuBifResetPdFree:
	push {r3, lr}
	ldr.w r0, [r0, #0x7c8]
	bl SceSysmemForDriver_009E1C61
	pop {r3, pc}
.size MmuBifResetPdFree, .-MmuBifResetPdFree
.align 2
.align 2
.global BM_DestroyHeap
.type BM_DestroyHeap, %function
.thumb_func
BM_DestroyHeap:
	push {r3, lr}
	mov r1, r0
	cbz r0, .LBM_DestroyHeape
	ldr r0, [r0, #0x18]
	cbz r0, .LBM_DestroyHeape
	bl RemoveGpuHeapFromMemoryContext
.LBM_DestroyHeape:
	pop {r3, pc}
.size BM_DestroyHeap, .-BM_DestroyHeap
.align 2
.align 2
.global BM_DestroyContext
.type BM_DestroyContext, %function
.thumb_func
BM_DestroyContext:
	push {r3, r4, r5, lr}
	mov r5, r0
	mov r4, r1
	cbz r1, .LBM_DestroyContextc
	movs r3, #0
	str r3, [r1]
.LBM_DestroyContextc:
	cbz r5, .LBM_DestroyContext24
	bl sceKernelGetProcessId
	mov r1, r0
	mov r0, r5
	bl RemoveMemoryContextFromList
	cbz r4, .LBM_DestroyContext20
	movs r3, #1
	str r3, [r4]
.LBM_DestroyContext20:
	movs r0, #0
	pop {r3, r4, r5, pc}
.LBM_DestroyContext24:
	movs r0, #3
	pop {r3, r4, r5, pc}
.size BM_DestroyContext, .-BM_DestroyContext
.align 2
.align 2
.global GpuRequestDestroy
.type GpuRequestDestroy, %function
.thumb_func
GpuRequestDestroy:
	ldr r3, GpuRequestDestroyLiteral28
	mov r2, r0
	push {r4, lr}
	ldr r1, GpuRequestDestroyLiteral2c
	ldr r3, [r3]
	ldr r0, [r3, #0x20]
	bl GpuRequestListFindVariadic
	mov r4, r0
	cbz r0, .LGpuRequestDestroy24
	bl GpuRequestListUnlink
	movs r0, #1
	movs r1, #0x28
	mov r2, r4
	movs r3, #0
	bl GpuFreeFlags
.LGpuRequestDestroy24:
	movs r0, #0
	pop {r4, pc}
.size GpuRequestDestroy, .-GpuRequestDestroy
.align 2
GpuRequestDestroyLiteral28:
	.word gpuDumpRegion + 0x3178
GpuRequestDestroyLiteral2c:
	.word GpuDeviceMatchFlags
.align 2
.global GpuLookupPlatformDevice
.type GpuLookupPlatformDevice, %function
.thumb_func
GpuLookupPlatformDevice:
	cmp r0, #7
	mov.w r0, #0
	itt eq
	ldreq r3, GpuLookupPlatformDeviceLiteral10
	streq r3, [r1]
	bx lr
	nop 
.size GpuLookupPlatformDevice, .-GpuLookupPlatformDevice
.align 2
GpuLookupPlatformDeviceLiteral10:
	.word gpuDumpRegion + 0x313c
#if FW_DEBUG
.align 2
.global VBAppend
.type VBAppend, %function
.thumb_func
VBAppend:
	push {r0, r1, r2, r3}
	push.w {r0, r1, r4, r5, r6, r7, r8, sb, sl, lr}
	add r3, sp, #0x28
	mov r6, lr
	ldr r7, VBAppendLiteral9c
	ldr sl, [r3], #4
	ldr r4, VBAppendLiterala0
	ldr r2, [r7]
	ldr r0, VBAppendLiterala4
	str r3, [sp]
	str r2, [sp, #4]
	bl SceCpuForDriver_D32ACE9E
	mov.w r2, #0x200
	ldr r1, VBAppendLiterala8
	mov r8, r0
	mov r0, r4
	bl strncpy
	movw r1, #0x201
	mov r0, r4
	bl strnlen
	ldr.w sb, [sp]
	mov r5, r0
	cmp.w r0, #0x200
	mov r2, r6
	rsb.w r6, r5, #0x200
	ite hi
	movhi r0, #0
	movls r0, #1
	ldr r1, VBAppendLiteralac
	adds r6, #1
	bl SceDebugForDriver_35A35322
	mov r3, sb
	adds r0, r4, r5
	mov r1, r6
	mov r2, sl
	bl vsnprintf
	cmp r0, #0
	mov.w r3, #0
	strb.w r3, [r4, #0x200]
	blt .LVBAppend76
	cmp r0, r6
	bge .LVBAppend76
	ldr r0, VBAppendLiteralb0
	mov r1, r4
	b .LVBAppend7a
.LVBAppend76:
	ldr r0, VBAppendLiteralb4
	ldr r1, VBAppendLiterala0
.LVBAppend7a:
	bl SceDebugForDriver_391B74B7
	ldr r0, VBAppendLiterala4
	mov r1, r8
	bl sceKernelSpinlockLowUnlockCpuResumeIntr
	ldr r2, [sp, #4]
	ldr r3, [r7]
	cmp r2, r3
	beq .LVBAppend92
	bl __stack_chk_fail
.LVBAppend92:
	add sp, #8
	pop.w {r4, r5, r6, r7, r8, sb, sl, lr}
	add sp, #0x10
	bx lr
.size VBAppend, .-VBAppend
.align 2
VBAppendLiteral9c:
	.word __stack_chk_guard
VBAppendLiterala0:
	.word gpuConsoleBuffer
VBAppendLiterala4:
	.word gpuConsoleLock
VBAppendLiterala8:
	.word sgxConstantString33
VBAppendLiteralac:
	.word sgxDebugInfo17
VBAppendLiteralb0:
	.word sgxConstantString35
VBAppendLiteralb4:
	.word sgxConstantString36
#endif
.align 2
.global SgxWaitFirmwareCommand
.type SgxWaitFirmwareCommand, %function
.thumb_func
SgxWaitFirmwareCommand:
	push.w {r4, r5, r6, r7, r8, sb, lr}
	sub sp, #0x3c
	ldr r7, SgxWaitFirmwareCommandLiterala8
	mov r5, r0
	mov r8, r1
	mov r6, r2
	ldr r4, [r0]
	ldr r3, [r7]
	str r3, [sp, #0x34]
	cmp r4, #0
	beq .LSgxWaitFirmwareCommand90
	ldr r3, [r4]
	mov r0, r4
	movs r1, #0
	bic r2, r3, #1
	stm r0!, {r2}
	movs r2, #0x20
	bl OSMemSet
	add.w r0, r4, #0x24
	movs r1, #0
	movs r2, #0x3c
	bl OSMemSet
	ldr r3, [r5, #8]
	mov r0, r8
	movs r1, #6
	add r2, sp, #0x14
	str r3, [sp, #0x20]
	movs r3, #0
	stm.w sp, {r3, r6}
	str r3, [sp, #8]
	mov.w r3, #-1
	bl GpuRuntimeSubmitCommand
	mov r6, r0
	cbnz r0, .LSgxWaitFirmwareCommand96
	bl GpuClockLow
	mov.w r8, #1
	mov r5, r0
	adds r0, #1
.LSgxWaitFirmwareCommand60:
	subs r3, r6, r5
	add r0, r3
	ldr r3, SgxWaitFirmwareCommandLiteralac
	cmp r0, r3
	bhi .LSgxWaitFirmwareCommand80
.LSgxWaitFirmwareCommand6a:
	ldr r3, [r4]
	ands sb, r3, #1
	bne .LSgxWaitFirmwareCommand94
	bl GpuClockLow
	cmp r0, r5
	itt lo
	mvnlo r6, r5
	movlo r5, sb
	b .LSgxWaitFirmwareCommand60
.LSgxWaitFirmwareCommand80:
	cmp.w r8, #0
	beq .LSgxWaitFirmwareCommand8c
	mov.w r8, #0
	b .LSgxWaitFirmwareCommand6a
.LSgxWaitFirmwareCommand8c:
	movs r0, #9
	b .LSgxWaitFirmwareCommand96
.LSgxWaitFirmwareCommand90:
	movs r0, #3
	b .LSgxWaitFirmwareCommand96
.LSgxWaitFirmwareCommand94:
	movs r0, #0
.LSgxWaitFirmwareCommand96:
	ldr r2, [sp, #0x34]
	ldr r3, [r7]
	cmp r2, r3
	beq .LSgxWaitFirmwareCommanda2
	bl __stack_chk_fail
.LSgxWaitFirmwareCommanda2:
	add sp, #0x3c
	pop.w {r4, r5, r6, r7, r8, sb, pc}
.size SgxWaitFirmwareCommand, .-SgxWaitFirmwareCommand
.align 2
SgxWaitFirmwareCommandLiterala8:
	.word __stack_chk_guard
SgxWaitFirmwareCommandLiteralac:
	.word 0x7a11f
.align 2
.global GpuReadRegister
.type GpuReadRegister, %function
.thumb_func
GpuReadRegister:
	ldr r0, [r0, r1]
	bx lr
.size GpuReadRegister, .-GpuReadRegister
.align 2
.align 2
.global GpuWriteRegister
.type GpuWriteRegister, %function
.thumb_func
GpuWriteRegister:
	str r2, [r0, r1]
	bx lr
.size GpuWriteRegister, .-GpuWriteRegister
.align 2
.align 2
.global GpuGetLockupEvent
.type GpuGetLockupEvent, %function
.thumb_func
GpuGetLockupEvent:
	ldr r3, GpuGetLockupEventLiteral8
	ldr r0, [r3]
	bx lr
.size GpuGetLockupEvent, .-GpuGetLockupEvent
.align 2
GpuGetLockupEventLiteral8:
	.word lockupEventUid
.align 2
.global GpuRuntimePendingDrain
.type GpuRuntimePendingDrain, %function
.thumb_func
GpuRuntimePendingDrain:
	push {r4, r5, r6, lr}
	mov r5, r0
	ldr r3, [r0, #0x54]
	mov r6, r1
	ldr.w r4, [r3, #0x7f0]
	ldr r3, [r4, #0x20]
	lsls r2, r3, #0x1e
	bpl .LGpuRuntimePendingDrain5c
	ldr r0, [r4, #0x24]
	ands r3, r0, #2
	bne .LGpuRuntimePendingDrain5c
	orr r0, r0, #2
	movs r1, #2
	str r0, [r4, #0x24]
	mov r2, r6
	ldr r0, [r5, #8]
	bl GpuRuntimeDrainRequests
	cbnz r0, .LGpuRuntimePendingDrain50
	ldr r3, [r5, #0x54]
	ldr.w r3, [r3, #0x7f0]
	ldr r2, [r3, #0x2c]
	adds r2, #1
	str r2, [r3, #0x2c]
	ldr r3, [r3, #4]
	lsls r3, r3, #0x1b
	bpl .LGpuRuntimePendingDrain5c
	adds r6, #3
	bne .LGpuRuntimePendingDrain48
	movs r3, #1
	str r3, [r5, #0x3c]
	pop {r4, r5, r6, pc}
.LGpuRuntimePendingDrain48:
	mov r0, r5
	bl GpuRuntimeSuspendCommand
	pop {r4, r5, r6, pc}
.LGpuRuntimePendingDrain50:
	cmp r0, #0x18
	bne .LGpuRuntimePendingDrain5c
	ldr r3, [r4, #0x24]
	bic r3, r3, #2
	str r3, [r4, #0x24]
.LGpuRuntimePendingDrain5c:
	pop {r4, r5, r6, pc}
.size GpuRuntimePendingDrain, .-GpuRuntimePendingDrain
.align 2
.align 2
.global GpuRuntimeSuspendCommand
.type GpuRuntimeSuspendCommand, %function
.thumb_func
GpuRuntimeSuspendCommand:
	push {r4, r5, r6, lr}
	sub sp, #0x38
	ldr r5, GpuRuntimeSuspendCommandLiteral50
	mov r4, r0
	movs r1, #0
	movs r2, #0x20
	ldr r3, [r5]
	str r3, [sp, #0x34]
	ldr r3, [r0, #0x54]
	add r0, sp, #0x14
	ldr.w r3, [r3, #0x7ec]
	ldr r6, [r3]
	bl memset
	ldr r3, [r6, #4]
	ands r3, r3, #0x20
	bne .LGpuRuntimeSuspendCommand3c
	strd r3, r3, [sp]
	mov r0, r4
	str r3, [sp, #8]
	movs r1, #7
	add r2, sp, #0x14
	mvn r3, #2
	bl GpuRuntimeSubmitCommand
	b .LGpuRuntimeSuspendCommand3e
.LGpuRuntimeSuspendCommand3c:
	movs r0, #0
.LGpuRuntimeSuspendCommand3e:
	ldr r2, [sp, #0x34]
	ldr r3, [r5]
	cmp r2, r3
	beq .LGpuRuntimeSuspendCommand4a
	bl __stack_chk_fail
.LGpuRuntimeSuspendCommand4a:
	add sp, #0x38
	pop {r4, r5, r6, pc}
.size GpuRuntimeSuspendCommand, .-GpuRuntimeSuspendCommand
.align 2
GpuRuntimeSuspendCommandLiteral50:
	.word __stack_chk_guard
.align 2
.global GpuSetLockupDevice
.type GpuSetLockupDevice, %function
.thumb_func
GpuSetLockupDevice:
	ldr r3, GpuSetLockupDeviceLiteral8
	str r0, [r3]
	bx lr
.size GpuSetLockupDevice, .-GpuSetLockupDevice
.align 2
GpuSetLockupDeviceLiteral8:
	.word gpuLockupDevice
#if FW_DEBUG
.align 2
.global RemoveMemoryContextFromList
.type RemoveMemoryContextFromList, %function
.thumb_func
RemoveMemoryContextFromList:
	push.w {r4, r5, r6, r7, r8, lr}
	mov r7, lr
	mov r4, r0
	cmp r0, #0
	beq .LRemoveMemoryContextFromList96
	mov r0, r1
	bl FindMemoryContext
	cmp r4, r0
	bne .LRemoveMemoryContextFromList96
	add.w r8, r4, #0x1000
	ldr.w r5, [r8, #0x18]
.LRemoveMemoryContextFromList1e:
	cbz r5, .LRemoveMemoryContextFromList3e
	ldr r6, [r5, #0x14]
.LRemoveMemoryContextFromList22:
	cbz r6, .LRemoveMemoryContextFromList32
	ldr r1, [r6]
	adds r0, r4, #4
	ldr r2, [r6, #0x10]
	bl sceGpuMmuUnmapMemory
	ldr r6, [r6, #0x2c]
	b .LRemoveMemoryContextFromList22
.LRemoveMemoryContextFromList32:
	ldr r6, [r5, #0x20]
	mov r0, r5
	bl DestroyGpuHeap
	mov r5, r6
	b .LRemoveMemoryContextFromList1e
.LRemoveMemoryContextFromList3e:
	str.w r5, [r8, #0x18]
	adds r0, r4, #4
	bl GpuMmuDestroyContext
	movs r0, #1
	ldr r1, RemoveMemoryContextFromListLiterala0
	mov r2, r7
	bl SceDebugForDriver_35A35322
	ldr r3, RemoveMemoryContextFromListLiterala4
	ldr r0, [r3]
	mov r6, r3
.LRemoveMemoryContextFromList58:
	cbz r0, .LRemoveMemoryContextFromList68
	cmp r0, r4
	beq .LRemoveMemoryContextFromList68
	add.w r3, r0, #0x1020
	mov r5, r0
	ldr r0, [r3]
	b .LRemoveMemoryContextFromList58
.LRemoveMemoryContextFromList68:
	adds r0, #0
	ldr r1, RemoveMemoryContextFromListLiterala8
	it ne
	movne r0, #1
	mov r2, r7
	bl SceDebugForDriver_35A35322
	add.w r3, r4, #0x1020
	cbnz r5, .LRemoveMemoryContextFromList82
	ldr r2, [r3]
	str r2, [r6]
	b .LRemoveMemoryContextFromList8a
.LRemoveMemoryContextFromList82:
	ldr r3, [r3]
	add.w r5, r5, #0x1020
	str r3, [r5]
.LRemoveMemoryContextFromList8a:
	mov r0, r4
	bl FreeProcessGpuMemory
	movs r0, #0
	pop.w {r4, r5, r6, r7, r8, pc}
.LRemoveMemoryContextFromList96:
	movs r0, #0
	movt r0, #0x804c
	pop.w {r4, r5, r6, r7, r8, pc}
.size RemoveMemoryContextFromList, .-RemoveMemoryContextFromList
.align 2
RemoveMemoryContextFromListLiterala0:
	.word sgxDebugInfo18
RemoveMemoryContextFromListLiterala4:
	.word processMemoryContexts
RemoveMemoryContextFromListLiterala8:
	.word sgxDebugInfo19
#else
.align 2
.global RemoveMemoryContextFromList
.type RemoveMemoryContextFromList, %function
.thumb_func
RemoveMemoryContextFromList:
	push {r3, r4, r5, r6, r7, lr}
	mov r4, r0
	cmp r0, #0
	beq .LRemoveMemoryContextFromList72
	mov r0, r1
	bl FindMemoryContext
	cmp r4, r0
	bne .LRemoveMemoryContextFromList72
	add.w r7, r4, #0x1000
	ldr r5, [r7, #0x18]
.LRemoveMemoryContextFromList18:
	cbz r5, .LRemoveMemoryContextFromList38
	ldr r6, [r5, #0x14]
.LRemoveMemoryContextFromList1c:
	cbz r6, .LRemoveMemoryContextFromList2c
	ldr r1, [r6]
	adds r0, r4, #4
	ldr r2, [r6, #0x10]
	bl sceGpuMmuUnmapMemory
	ldr r6, [r6, #0x2c]
	b .LRemoveMemoryContextFromList1c
.LRemoveMemoryContextFromList2c:
	ldr r6, [r5, #0x20]
	mov r0, r5
	bl DestroyGpuHeap
	mov r5, r6
	b .LRemoveMemoryContextFromList18
.LRemoveMemoryContextFromList38:
	str r5, [r7, #0x18]
	adds r0, r4, #4
	bl GpuMmuDestroyContext
	ldr r2, RemoveMemoryContextFromListLiteral7c
	ldr r3, [r2]
.LRemoveMemoryContextFromList44:
	cbz r3, .LRemoveMemoryContextFromList4a
	cmp r3, r4
	bne .LRemoveMemoryContextFromList56
.LRemoveMemoryContextFromList4a:
	add.w r3, r4, #0x1020
	cbnz r5, .LRemoveMemoryContextFromList60
	ldr r3, [r3]
	str r3, [r2]
	b .LRemoveMemoryContextFromList68
.LRemoveMemoryContextFromList56:
	add.w r1, r3, #0x1020
	mov r5, r3
	ldr r3, [r1]
	b .LRemoveMemoryContextFromList44
.LRemoveMemoryContextFromList60:
	ldr r3, [r3]
	add.w r5, r5, #0x1020
	str r3, [r5]
.LRemoveMemoryContextFromList68:
	mov r0, r4
	bl FreeProcessGpuMemory
	movs r0, #0
	pop {r3, r4, r5, r6, r7, pc}
.LRemoveMemoryContextFromList72:
	movs r0, #0
	movt r0, #0x804c
	pop {r3, r4, r5, r6, r7, pc}
.size RemoveMemoryContextFromList, .-RemoveMemoryContextFromList
.align 2
RemoveMemoryContextFromListLiteral7c:
	.word processMemoryContexts
#endif
.align 2
.global GpuRequestListFindVariadic
.type GpuRequestListFindVariadic, %function
.thumb_func
GpuRequestListFindVariadic:
	push {r1, r2, r3}
	mov r3, r0
	push {r0, r1, r2, r4, r5, r6, lr}
	movs r0, #0
	ldr r2, GpuRequestListFindVariadicLiteral3c
	ldr r5, [sp, #0x1c]
	ldr r1, [r2]
	mov r4, r2
	str r1, [sp, #4]
.LGpuRequestListFindVariadic12:
	cbnz r3, .LGpuRequestListFindVariadic20
.LGpuRequestListFindVariadic14:
	ldr r2, [sp, #4]
	ldr r3, [r4]
	cmp r2, r3
	beq .LGpuRequestListFindVariadic32
	bl __stack_chk_fail
.LGpuRequestListFindVariadic20:
	cmp r0, #0
	bne .LGpuRequestListFindVariadic14
	ldr r6, [r3, #0x20]
	add r1, sp, #0x20
	mov r0, r3
	str r1, [sp]
	blx r5
	mov r3, r6
	b .LGpuRequestListFindVariadic12
.LGpuRequestListFindVariadic32:
	add sp, #0xc
	pop.w {r4, r5, r6, lr}
	add sp, #0xc
	bx lr
.size GpuRequestListFindVariadic, .-GpuRequestListFindVariadic
.align 2
GpuRequestListFindVariadicLiteral3c:
	.word __stack_chk_guard
.align 2
.global GpuRuntimeSubmitCommand
.type GpuRuntimeSubmitCommand, %function
.thumb_func
GpuRuntimeSubmitCommand:
	push.w {r0, r1, r2, r3, r4, r5, r6, r7, r8, lr}
	mov r5, r0
	mov r8, r1
	mov r7, r2
	mov r6, r3
	mov r2, r3
	ldr r0, [r0, #8]
	movs r1, #0
	movs r3, #1
	bl GpuRuntimeDrainRequests
	cbnz r0, .LGpuRuntimeSubmitCommand40
	ldr r3, [sp, #0x28]
	mov r1, r8
	str r0, [r5, #0x3c]
	mov r2, r7
	mov r0, r5
	str r3, [sp]
	ldr r3, [sp, #0x2c]
	str r3, [sp, #4]
	ldr r3, [sp, #0x30]
	str r3, [sp, #8]
	mov r3, r6
	bl GpuRuntimeQueueCommand
	mov r4, r0
	mov r0, r6
	bl GpuRuntimeCheckEnd
	mov r0, r4
	b .LGpuRuntimeSubmitCommand56
.LGpuRuntimeSubmitCommand40:
	cmp r0, #0x18
	bne .LGpuRuntimeSubmitCommand56
	adds r6, #3
	bne .LGpuRuntimeSubmitCommand56
	movs r3, #1
	str r3, [r5, #0x3c]
	ldr r3, GpuRuntimeSubmitCommandLiteral5c
	ldr r0, [r3]
	bl GpuRuntimeStateSignal
	movs r0, #0
.LGpuRuntimeSubmitCommand56:
	add sp, #0x10
	pop.w {r4, r5, r6, r7, r8, pc}
.size GpuRuntimeSubmitCommand, .-GpuRuntimeSubmitCommand
.align 2
GpuRuntimeSubmitCommandLiteral5c:
	.word gpuDumpRegion + 0x3178
.align 2
.global GpuClockLow
.type GpuClockLow, %function
.thumb_func
GpuClockLow:
	push {r3, lr}
	bl SceThreadmgrForDriver_47F6DE49
	pop {r3, pc}
.size GpuClockLow, .-GpuClockLow
.align 2
#if FW_DEBUG
.align 2
.global sceGpuMmuUnmapMemory
.type sceGpuMmuUnmapMemory, %function
.thumb_func
sceGpuMmuUnmapMemory:
	push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr}
	mov r7, lr
	sub sp, #0x1c
	mov r6, r1
	mov r5, r2
	mov sl, r0
	cmp r0, #0
	beq.w .LsceGpuMmuUnmapMemory152
	ubfx r3, r1, #0, #0xc
	cmp r3, #0
	bne.w .LsceGpuMmuUnmapMemory152
	ubfx r4, r2, #0, #0xc
	cmp r4, #0
	bne.w .LsceGpuMmuUnmapMemory152
	movs r3, #1
	str r3, [sp]
	bl CheckGpuMmuRange
	cmp r0, #0
	blt.w .LsceGpuMmuUnmapMemory158
	str r4, [sp, #0xc]
.LsceGpuMmuUnmapMemory38:
	cmp r5, #0
	beq.w .LsceGpuMmuUnmapMemory142
	lsr.w fp, r6, #0x16
	mov r2, r7
	ubfx r8, r6, #0, #0x16
	add.w r1, sl, fp, lsl #2
	rsb.w sb, r8, #0x400000
	ldr r4, [r1, #0x14]
	str r1, [sp, #0x14]
	ldr r1, sceGpuMmuUnmapMemoryLiteral160
	adds r0, r4, #0
	it ne
	movne r0, #1
	bl SceDebugForDriver_35A35322
	ldr.w r3, [sl, #4]
	mov r2, r7
	ldr r1, sceGpuMmuUnmapMemoryLiteral164
	ldr.w r0, [r3, fp, lsl #2]
	adds r0, #0
	it ne
	movne r0, #1
	cmp sb, r5
	it hs
	movhs sb, r5
	bl SceDebugForDriver_35A35322
	ldr r3, [r4, #0xc]
	mov.w ip, #1
	orr.w r2, sb, r8
	lsl.w ip, ip, r3
	add.w ip, ip, #-1
	ands.w ip, r2, ip
	bne .LsceGpuMmuUnmapMemoryf6
	ldr r0, [r4, #0x10]
	lsr.w r8, r8, r3
	lsr.w r3, sb, r3
	mov r2, r7
	ldr r1, sceGpuMmuUnmapMemoryLiteral168
	add r3, r8
	str.w ip, [sp, #8]
	cmp r3, r0
	str r3, [sp, #0x10]
	ite hi
	movhi r0, #0
	movls r0, #1
	bl SceDebugForDriver_35A35322
	ldr.w ip, [sp, #8]
	lsl.w r3, r8, #2
	mov r2, ip
.LsceGpuMmuUnmapMemoryc0:
	ldr r1, [sp, #0x10]
	cmp r8, r1
	bhs .LsceGpuMmuUnmapMemoryec
	ldr r2, [r4, #8]
	add.w r8, r8, #1
	ldr r1, sceGpuMmuUnmapMemoryLiteral16c
	ldr r0, [r2, r3]
	mov r2, r7
	str r3, [sp, #8]
	adds r0, #0
	it ne
	movne r0, #1
	bl SceDebugForDriver_35A35322
	ldr r2, [r4, #8]
	movs r1, #0
	ldr r3, [sp, #8]
	str r1, [r2, r3]
	adds r3, #4
	movs r2, #1
	b .LsceGpuMmuUnmapMemoryc0
.LsceGpuMmuUnmapMemoryec:
	cbz r2, .LsceGpuMmuUnmapMemoryfe
	bl GpuMmuInvalidatePageDirectory
	movs r0, #0
	b .LsceGpuMmuUnmapMemory100
.LsceGpuMmuUnmapMemoryf6:
	movs r0, #0
	movt r0, #0x804c
	b .LsceGpuMmuUnmapMemory100
.LsceGpuMmuUnmapMemoryfe:
	mov r0, r2
.LsceGpuMmuUnmapMemory100:
	clz r0, r0
	ldr r1, sceGpuMmuUnmapMemoryLiteral170
	lsrs r0, r0, #5
	mov r2, r7
	bl SceDebugForDriver_35A35322
	movs r1, #0
	mov r0, r4
	str r1, [sp]
	mov.w r2, #0x400000
	movs r3, #1
	bl sceGpuMmuCheckPageTableEntries
	mov r8, r0
	cbnz r0, .LsceGpuMmuUnmapMemory13a
	mov r0, r4
	bl sceGpuMmuDestroyPageTable
	ldr r3, [sp, #0x14]
	movs r1, #1
	str r1, [sp, #0xc]
	str.w r8, [r3, #0x14]
	ldr.w r3, [sl, #4]
	str.w r8, [r3, fp, lsl #2]
.LsceGpuMmuUnmapMemory13a:
	add r6, sb
	sub.w r5, r5, sb
	b .LsceGpuMmuUnmapMemory38
.LsceGpuMmuUnmapMemory142:
	ldr r3, [sp, #0xc]
	cbz r3, .LsceGpuMmuUnmapMemory14a
	bl GpuMmuInvalidatePageTable
.LsceGpuMmuUnmapMemory14a:
	bl GpuMmuInvalidateCache
	movs r0, #0
	b .LsceGpuMmuUnmapMemory158
.LsceGpuMmuUnmapMemory152:
	movs r0, #0
	movt r0, #0x804c
.LsceGpuMmuUnmapMemory158:
	add sp, #0x1c
	pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc}
.size sceGpuMmuUnmapMemory, .-sceGpuMmuUnmapMemory
.align 2
sceGpuMmuUnmapMemoryLiteral160:
	.word sgxDebugInfo20
sceGpuMmuUnmapMemoryLiteral164:
	.word sgxDebugInfo21
sceGpuMmuUnmapMemoryLiteral168:
	.word sgxDebugInfo22
sceGpuMmuUnmapMemoryLiteral16c:
	.word sgxDebugInfo23
sceGpuMmuUnmapMemoryLiteral170:
	.word sgxDebugInfo24
#else
.align 2
.global sceGpuMmuUnmapMemory
.type sceGpuMmuUnmapMemory, %function
.thumb_func
sceGpuMmuUnmapMemory:
	push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr}
	mov r5, r1
	sub sp, #0x14
	mov r4, r2
	mov sb, r0
	cmp r0, #0
	beq .LsceGpuMmuUnmapMemorycc
	ubfx r3, r1, #0, #0xc
	cmp r3, #0
	bne .LsceGpuMmuUnmapMemorycc
	ubfx r6, r2, #0, #0xc
	cmp r6, #0
	bne .LsceGpuMmuUnmapMemorycc
	movs r3, #1
	str r3, [sp]
	bl CheckGpuMmuRange
	cmp r0, #0
	blt .LsceGpuMmuUnmapMemoryd2
	mov r8, r6
.LsceGpuMmuUnmapMemory2e:
	cmp r4, #0
	beq .LsceGpuMmuUnmapMemoryba
	lsr.w sl, r5, #0x16
	ubfx r2, r5, #0, #0x16
	movs r0, #1
	add.w fp, sb, sl, lsl #2
	rsb.w r6, r2, #0x400000
	cmp r6, r4
	it hs
	movhs r6, r4
	ldr.w r7, [fp, #0x14]
	orr.w r1, r6, r2
	ldr r3, [r7, #0xc]
	lsls r0, r3
	subs r0, #1
	ands r0, r1
	bne .LsceGpuMmuUnmapMemory86
	lsrs r2, r3
	lsr.w lr, r6, r3
	lsls r1, r2, #2
	mov r3, r2
.LsceGpuMmuUnmapMemory66:
	add.w ip, lr, r2
	cmp r3, ip
	bhs .LsceGpuMmuUnmapMemory80
	ldr.w ip, [r7, #8]
	movs r0, #0
	adds r3, #1
	str.w r0, [ip, r1]
	adds r1, #4
	movs r0, #1
	b .LsceGpuMmuUnmapMemory66
.LsceGpuMmuUnmapMemory80:
	cbz r0, .LsceGpuMmuUnmapMemory86
	bl GpuMmuInvalidatePageDirectory
.LsceGpuMmuUnmapMemory86:
	movs r1, #0
	movs r3, #1
	str r1, [sp]
	mov r0, r7
	mov.w r2, #0x400000
	bl sceGpuMmuCheckPageTableEntries
	mov r3, r0
	cbnz r0, .LsceGpuMmuUnmapMemoryb4
	mov r0, r7
	str r3, [sp, #0xc]
	bl sceGpuMmuDestroyPageTable
	ldr r3, [sp, #0xc]
	mov.w r8, #1
	str.w r3, [fp, #0x14]
	ldr.w r2, [sb, #4]
	str.w r3, [r2, sl, lsl #2]
.LsceGpuMmuUnmapMemoryb4:
	add r5, r6
	subs r4, r4, r6
	b .LsceGpuMmuUnmapMemory2e
.LsceGpuMmuUnmapMemoryba:
	cmp.w r8, #0
	beq .LsceGpuMmuUnmapMemoryc4
	bl GpuMmuInvalidatePageTable
.LsceGpuMmuUnmapMemoryc4:
	bl GpuMmuInvalidateCache
	movs r0, #0
	b .LsceGpuMmuUnmapMemoryd2
.LsceGpuMmuUnmapMemorycc:
	movs r0, #0
	movt r0, #0x804c
.LsceGpuMmuUnmapMemoryd2:
	add sp, #0x14
	pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc}
.size sceGpuMmuUnmapMemory, .-sceGpuMmuUnmapMemory
.align 2
#endif
.align 2
.global GpuMmuDestroyContext
.type GpuMmuDestroyContext, %function
.thumb_func
GpuMmuDestroyContext:
	push {r4, r5, r6, lr}
	mov r5, r0
	movs r4, #0
.LGpuMmuDestroyContext6:
	adds r6, r5, r4
	ldr r0, [r6, #0x14]
	cbz r0, .LGpuMmuDestroyContext14
	bl sceGpuMmuDestroyPageTable
	movs r3, #0
	str r3, [r6, #0x14]
.LGpuMmuDestroyContext14:
	adds r4, #4
	cmp.w r4, #0x1000
	bne .LGpuMmuDestroyContext6
	mov r0, r5
	bl FreeGpuPageDirectory
	add.w r0, r5, #0xc
	bl FreeGpuPageTablePool
	movs r0, #0
	pop {r4, r5, r6, pc}
.size GpuMmuDestroyContext, .-GpuMmuDestroyContext
.align 2
.align 2
.global GpuRuntimeQueueCommand
.type GpuRuntimeQueueCommand, %function
.thumb_func
GpuRuntimeQueueCommand:
	push.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
	mov sl, r1
	ldr r6, [r0, #0x54]
	mov r5, r2
	mov.w sb, #1
	mov.w fp, #0
	ldr r7, [r6, #0x40]
	bl GpuClockLow
	mov r4, r0
	adds r0, #1
.LGpuRuntimeQueueCommand1c:
	sub.w r3, fp, r4
	add r0, r3
	ldr r3, GpuRuntimeQueueCommandLiterale0
	cmp r0, r3
	bhi .LGpuRuntimeQueueCommand5e
.LGpuRuntimeQueueCommand28:
	ldr r3, [r7, #0xc]
	ldr r2, [r3]
	ldr r3, [r7, #0x10]
	adds r1, r2, #1
	ldr r3, [r3]
	uxtb r1, r1
	cmp r1, r3
	beq .LGpuRuntimeQueueCommand48
	ldr.w sb, [r7, #8]
	lsl.w r8, r2, #5
	adds.w r4, sb, r8
	bne .LGpuRuntimeQueueCommand6a
	b .LGpuRuntimeQueueCommandd4
.LGpuRuntimeQueueCommand48:
	movs r0, #1
	bl GpuDelayClock
	bl GpuClockLow
	cmp r0, r4
	itt lo
	mvnlo.w fp, r4
	movlo r4, #0
	b .LGpuRuntimeQueueCommand1c
.LGpuRuntimeQueueCommand5e:
	cmp.w sb, #0
	beq .LGpuRuntimeQueueCommandd4
	mov.w sb, #0
	b .LGpuRuntimeQueueCommand28
.LGpuRuntimeQueueCommand6a:
	ldr.w r3, [r6, #0x94]
	cmp.w sl, #0xa
	str r3, [r5, #4]
	mov.w r3, #0
	str.w r3, [r6, #0x94]
	ldm r5!, {r0, r1, r2, r3}
	stm r4!, {r0, r1, r2, r3}
	ldm.w r5, {r0, r1, r2, r3}
	stm.w r4, {r0, r1, r2, r3}
	bgt .LGpuRuntimeQueueCommandda
	add.w sl, sl, #0x16
	bl SceCpuForDriver_E813EBB2
	ldr.w r3, [r6, sl, lsl #2]
	str.w r3, [sb, r8]
	bl SceCpuForDriver_E813EBB2
	ldr r3, [r7, #0xc]
	ldr r2, [r3]
	adds r2, #1
	uxtb r2, r2
	str r2, [r3]
	bl SceCpuForDriver_E813EBB2
	ldr r3, [r6, #0x50]
	ldr r2, [r3]
	adds r2, #1
	uxtb r2, r2
	str r2, [r3]
	bl SceCpuForDriver_E813EBB2
	dmb sy
	movw r1, #0x8ac8
	ldr r0, [r6, #0x14]
	movs r2, #1
	bl GpuWriteRegister
	dsb sy
	movs r0, #0
	pop.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, pc}
.LGpuRuntimeQueueCommandd4:
	movs r0, #9
	pop.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, pc}
.LGpuRuntimeQueueCommandda:
	movs r0, #0x69
	pop.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, pc}
.size GpuRuntimeQueueCommand, .-GpuRuntimeQueueCommand
.align 2
GpuRuntimeQueueCommandLiterale0:
	.word 0x7a11f
.align 2
.global GpuMmuInvalidatePageDirectory
.type GpuMmuInvalidatePageDirectory, %function
.thumb_func
GpuMmuInvalidatePageDirectory:
	push {r3, lr}
	movs r0, #1
	bl GpuSgxInvalidateFlags
	bl GpuMmuInvalidateCache
	pop {r3, pc}
.size GpuMmuInvalidatePageDirectory, .-GpuMmuInvalidatePageDirectory
.align 2
#if FW_DEBUG
.align 2
.global sceGpuMmuDestroyPageTable
.type sceGpuMmuDestroyPageTable, %function
.thumb_func
sceGpuMmuDestroyPageTable:
	ldr r3, sceGpuMmuDestroyPageTableLiteral80
	push {r4, r5, r6, lr}
	sub sp, #0x30
	mov r5, lr
	mov r4, r0
	mov r6, r3
	ldr r2, [r3]
	str r2, [sp, #0x2c]
	cbz r0, .LsceGpuMmuDestroyPageTable68
	ldr r0, [r0, #4]
	mov r2, lr
	ldr r1, sceGpuMmuDestroyPageTableLiteral84
	adds r0, #0
	it ne
	movne r0, #1
	bl SceDebugForDriver_35A35322
	ldr r3, [r4, #4]
	mov r2, r5
	ldr r1, sceGpuMmuDestroyPageTableLiteral88
	ldr r0, [r3, #4]
	adds r0, #0
	it ne
	movne r0, #1
	bl SceDebugForDriver_35A35322
	ldr r3, [r4, #4]
	add r2, sp, #0x10
	ldr r1, [r4, #8]
	ldr r0, [r3, #4]
	bl sceGpuHeapFreeByGpuVA
	subs r5, r0, #0
	bge .LsceGpuMmuDestroyPageTable5e
	ldr r3, sceGpuMmuDestroyPageTableLiteral8c
	movs r0, #1
	movs r1, #0
	ldr r2, sceGpuMmuDestroyPageTableLiteral90
	str r3, [sp]
	ldr r3, [r4, #8]
	strd r3, r5, [sp, #4]
	ldr r3, sceGpuMmuDestroyPageTableLiteral94
	bl SceDebugForDriver_D9703808
	mov r0, r5
	b .LsceGpuMmuDestroyPageTable6e
.LsceGpuMmuDestroyPageTable5e:
	mov r0, r4
	bl FreeProcessGpuMemory
	movs r0, #0
	b .LsceGpuMmuDestroyPageTable6e
.LsceGpuMmuDestroyPageTable68:
	movs r0, #0
	movt r0, #0x804c
.LsceGpuMmuDestroyPageTable6e:
	ldr r2, [sp, #0x2c]
	ldr r3, [r6]
	cmp r2, r3
	beq .LsceGpuMmuDestroyPageTable7a
	bl __stack_chk_fail
.LsceGpuMmuDestroyPageTable7a:
	add sp, #0x30
	pop {r4, r5, r6, pc}
.size sceGpuMmuDestroyPageTable, .-sceGpuMmuDestroyPageTable
.align 2
sceGpuMmuDestroyPageTableLiteral80:
	.word __stack_chk_guard
sceGpuMmuDestroyPageTableLiteral84:
	.word sgxDebugInfo25
sceGpuMmuDestroyPageTableLiteral88:
	.word sgxDebugInfo26
sceGpuMmuDestroyPageTableLiteral8c:
	.word sgxConstantString39
sceGpuMmuDestroyPageTableLiteral90:
	.word sgxDebugInfo27
sceGpuMmuDestroyPageTableLiteral94:
	.word sgxConstantString40
#else
.align 2
.global sceGpuMmuDestroyPageTable
.type sceGpuMmuDestroyPageTable, %function
.thumb_func
sceGpuMmuDestroyPageTable:
	ldr r3, sceGpuMmuDestroyPageTableLiteral40
	push {r4, r5, lr}
	sub sp, #0x24
	mov r4, r0
	mov r5, r3
	ldr r2, [r3]
	str r2, [sp, #0x1c]
	cbz r0, .LsceGpuMmuDestroyPageTable2a
	ldr r3, [r0, #4]
	mov r2, sp
	ldr r1, [r4, #8]
	ldr r0, [r3, #4]
	bl sceGpuHeapFreeByGpuVA
	cmp r0, #0
	blt .LsceGpuMmuDestroyPageTable30
	mov r0, r4
	bl FreeProcessGpuMemory
	movs r0, #0
	b .LsceGpuMmuDestroyPageTable30
.LsceGpuMmuDestroyPageTable2a:
	movs r0, #0
	movt r0, #0x804c
.LsceGpuMmuDestroyPageTable30:
	ldr r2, [sp, #0x1c]
	ldr r3, [r5]
	cmp r2, r3
	beq .LsceGpuMmuDestroyPageTable3c
	bl __stack_chk_fail
.LsceGpuMmuDestroyPageTable3c:
	add sp, #0x24
	pop {r4, r5, pc}
.size sceGpuMmuDestroyPageTable, .-sceGpuMmuDestroyPageTable
.align 2
sceGpuMmuDestroyPageTableLiteral40:
	.word __stack_chk_guard
#endif
.align 2
.global GpuMmuInvalidatePageTable
.type GpuMmuInvalidatePageTable, %function
.thumb_func
GpuMmuInvalidatePageTable:
	push {r3, lr}
	movs r0, #2
	bl GpuSgxInvalidateFlags
	bl GpuMmuInvalidateCache
	pop {r3, pc}
.size GpuMmuInvalidatePageTable, .-GpuMmuInvalidatePageTable
.align 2
.align 2
.global GpuMmuInvalidateCache
.type GpuMmuInvalidateCache, %function
.thumb_func
GpuMmuInvalidateCache:
	push {r3, lr}
	movs r0, #4
	bl GpuSgxInvalidateFlags
	pop {r3, pc}
.size GpuMmuInvalidateCache, .-GpuMmuInvalidateCache
.align 2
.align 2
.global GpuDelayClock
.type GpuDelayClock, %function
.thumb_func
GpuDelayClock:
	push {r3, lr}
	mov.w r3, #0x3e8
	muls r0, r3, r0
	bl sceKernelDelayThread
	pop {r3, pc}
.size GpuDelayClock, .-GpuDelayClock
.align 2
.align 2
.global GpuSgxInvalidateFlags
.type GpuSgxInvalidateFlags, %function
.thumb_func
GpuSgxInvalidateFlags:
	push {r4, lr}
	mov r4, r0
	bl GpuGetLockupDevice
	ldr r3, [r0, #0x54]
	ldr.w r2, [r3, #0x94]
	orrs r2, r4
	str.w r2, [r3, #0x94]
	pop {r4, pc}
.size GpuSgxInvalidateFlags, .-GpuSgxInvalidateFlags
.align 2
.align 2
.global GpuGetLockupDevice
.type GpuGetLockupDevice, %function
.thumb_func
GpuGetLockupDevice:
	ldr r3, GpuGetLockupDeviceLiteral8
	ldr r0, [r3]
	bx lr
.size GpuGetLockupDevice, .-GpuGetLockupDevice
.align 2
GpuGetLockupDeviceLiteral8:
	.word gpuLockupDevice
.section .rodata
.global sgxConstantString0
sgxConstantString0:
	.byte 100,101,118,105,99,101,115,47,115,103,120,47,115,103,120,105,110,105,116,46,99,0
.global sgxConstantString1
sgxConstantString1:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,68,101,118,105,99,101,32,110,111,116,32,111,102,32,116,121,112,101,32,83,71,88,0
.global sgxConstantString2
sgxConstantString2:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,77,105,115,109,97,116,99,104,32,105,110,32,99,108,105,101,110,116,45,115,105,100,101,32,97,110,100,32,75,77,32,100,114,105,118,101,114,32,98,117,105,108,100,32,111,112,116,105,111,110,115,59,32,101,120,116,114,97,32,111,112,116,105,111,110,115,32,112,114,101,115,101,110,116,32,105,110,32,99,108,105,101,110,116,45,115,105,100,101,32,100,114,105,118,101,114,58,32,40,48,120,37,120,41,46,32,80,108,101,97,115,101,32,99,104,101,99,107,32,115,103,120,95,111,112,116,105,111,110,115,46,104,0
.global sgxConstantString3
sgxConstantString3:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,77,105,115,109,97,116,99,104,32,105,110,32,99,108,105,101,110,116,45,115,105,100,101,32,97,110,100,32,75,77,32,100,114,105,118,101,114,32,98,117,105,108,100,32,111,112,116,105,111,110,115,59,32,101,120,116,114,97,32,111,112,116,105,111,110,115,32,112,114,101,115,101,110,116,32,105,110,32,75,77,58,32,40,48,120,37,120,41,46,32,80,108,101,97,115,101,32,99,104,101,99,107,32,115,103,120,95,111,112,116,105,111,110,115,46,104,0
.global sgxConstantString4
sgxConstantString4:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,85,110,97,98,108,101,32,116,111,32,118,97,108,105,100,97,116,101,32,100,101,118,105,99,101,32,68,68,75,32,118,101,114,115,105,111,110,0
.global sgxConstantString5
sgxConstantString5:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,73,110,99,111,109,112,97,116,105,98,108,101,32,100,114,105,118,101,114,32,68,68,75,32,114,101,118,105,115,105,111,110,32,40,37,100,41,47,100,101,118,105,99,101,32,68,68,75,32,114,101,118,105,115,105,111,110,32,40,37,100,41,46,0
.global sgxConstantString6
sgxConstantString6:
	.byte 83,71,88,73,110,105,116,58,32,72,87,32,99,111,114,101,32,114,101,118,32,40,37,120,41,44,32,83,87,32,99,111,114,101,32,114,101,118,32,40,37,120,41,32,99,104,101,99,107,32,115,107,105,112,112,101,100,46,0
.global sgxConstantString7
sgxConstantString7:
	.byte 83,71,88,73,110,105,116,58,32,72,87,32,99,111,114,101,32,114,101,118,32,40,37,120,41,32,99,104,101,99,107,32,115,107,105,112,112,101,100,46,0
.global sgxConstantString8
sgxConstantString8:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,77,105,115,109,97,116,99,104,32,105,110,32,83,71,88,77,75,73,70,32,115,116,114,117,99,116,117,114,101,32,115,105,122,101,115,46,0
.global sgxConstantString9
sgxConstantString9:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,77,105,115,109,97,116,99,104,32,105,110,32,100,114,105,118,101,114,32,97,110,100,32,109,105,99,114,111,107,101,114,110,101,108,32,98,117,105,108,100,32,111,112,116,105,111,110,115,59,32,101,120,116,114,97,32,111,112,116,105,111,110,115,32,112,114,101,115,101,110,116,32,105,110,32,100,114,105,118,101,114,58,32,40,48,120,37,120,41,46,32,80,108,101,97,115,101,32,99,104,101,99,107,32,115,103,120,95,111,112,116,105,111,110,115,46,104,0
.global sgxConstantString10
sgxConstantString10:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,77,105,115,109,97,116,99,104,32,105,110,32,100,114,105,118,101,114,32,97,110,100,32,109,105,99,114,111,107,101,114,110,101,108,32,98,117,105,108,100,32,111,112,116,105,111,110,115,59,32,101,120,116,114,97,32,111,112,116,105,111,110,115,32,112,114,101,115,101,110,116,32,105,110,32,109,105,99,114,111,107,101,114,110,101,108,58,32,40,48,120,37,120,41,46,32,80,108,101,97,115,101,32,99,104,101,99,107,32,115,103,120,95,111,112,116,105,111,110,115,46,104,0
.global sgxConstantString11
sgxConstantString11:
	.byte 40,70,65,73,76,41,32,83,71,88,73,110,105,116,58,32,73,110,99,111,109,112,97,116,105,98,108,101,32,72,87,32,99,111,114,101,32,114,101,118,32,40,37,120,41,32,97,110,100,32,83,87,32,99,111,114,101,32,114,101,118,32,40,37,120,41,46,0
.global sgxConstantString12
sgxConstantString12:
	.byte 99,111,109,109,111,110,47,98,117,102,102,101,114,95,109,97,110,97,103,101,114,95,115,99,101,46,99,0
.global sgxConstantString13
sgxConstantString13:
	.byte 66,77,95,67,114,101,97,116,101,67,111,110,116,101,120,116,0
.global sgxConstantString14
sgxConstantString14:
	.byte 37,115,58,32,77,105,115,115,105,110,103,32,100,101,118,105,99,101,32,110,111,100,101,32,105,110,102,111,114,109,97,116,105,111,110,46,10,0
.global sgxConstantString15
sgxConstantString15:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,105,110,105,116,105,97,108,105,115,101,32,71,80,85,32,109,101,109,111,114,121,32,99,111,110,116,101,120,116,115,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString16
sgxConstantString16:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,103,101,116,32,107,101,114,110,101,108,32,109,101,109,111,114,121,32,99,111,110,116,101,120,116,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString17
sgxConstantString17:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,99,114,101,97,116,101,32,107,101,114,110,101,108,32,112,97,103,101,32,116,97,98,108,101,32,112,111,111,108,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString18
sgxConstantString18:
	.byte 83,99,101,71,112,117,75,101,114,110,101,108,68,101,118,77,101,109,0
.global sgxConstantString19
sgxConstantString19:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,99,114,101,97,116,101,32,107,101,114,110,101,108,32,100,101,118,105,99,101,32,109,101,109,111,114,121,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString20
sgxConstantString20:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,114,101,103,105,115,116,101,114,32,107,101,114,110,101,108,32,100,101,118,105,99,101,32,109,101,109,111,114,121,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString21
sgxConstantString21:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,99,114,101,97,116,101,32,107,101,114,110,101,108,32,112,97,103,101,32,100,105,114,101,99,116,111,114,121,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString22
sgxConstantString22:
	.byte 83,99,101,71,112,117,0
.global sgxConstantString23
sgxConstantString23:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,114,101,103,105,115,116,101,114,32,107,101,114,110,101,108,32,112,114,111,99,101,115,115,32,109,101,109,111,114,121,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString24
sgxConstantString24:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,103,101,116,32,107,101,114,110,101,108,32,112,114,111,99,101,115,115,32,109,101,109,111,114,121,46,10,0
.global sgxConstantString25
sgxConstantString25:
	.byte 66,77,95,67,114,101,97,116,101,72,101,97,112,0
.global sgxConstantString26
sgxConstantString26:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,99,114,101,97,116,101,32,71,80,85,32,104,101,97,112,32,37,48,56,120,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString27
sgxConstantString27:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,97,100,100,32,71,80,85,32,104,101,97,112,32,37,48,56,120,32,116,111,32,109,101,109,111,114,121,32,99,111,110,116,101,120,116,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.global sgxConstantString28
sgxConstantString28:
	.byte 83,99,101,71,112,117,66,105,102,82,101,115,101,116,0
.global sgxConstantString29
sgxConstantString29:
	.byte 77,77,85,95,66,73,70,82,101,115,101,116,80,68,65,108,108,111,99,0
.global sgxConstantString30
sgxConstantString30:
	.byte 100,101,118,105,99,101,115,47,115,103,120,47,109,109,117,95,115,99,101,46,99,0
.global sgxConstantString31
sgxConstantString31:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,103,101,116,32,97,100,100,114,101,115,115,32,102,111,114,32,37,48,56,120,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,10,0
.global sgxConstantString32
sgxConstantString32:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,103,101,116,32,112,104,121,115,105,99,97,108,32,97,100,100,114,101,115,115,32,102,111,114,32,37,112,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,10,0
.global sgxConstantString33
sgxConstantString33:
	.byte 80,86,82,95,75,58,32,0
.global sgxConstantString34
sgxConstantString34:
	.byte 101,110,118,47,112,108,97,121,115,116,97,116,105,111,110,47,112,118,114,95,100,101,98,117,103,46,99,0
.global sgxConstantString35
sgxConstantString35:
	.byte 37,115,10,0
.global sgxConstantString36
sgxConstantString36:
	.byte 80,86,82,95,75,58,40,77,101,115,115,97,103,101,32,84,114,117,110,99,97,116,101,100,41,58,32,37,115,10,0
.global sgxConstantString37
sgxConstantString37:
	.byte 46,46,47,46,46,47,46,46,47,46,46,47,100,114,105,118,101,114,47,115,114,99,47,103,112,117,47,109,101,109,111,114,121,47,109,101,109,111,114,121,99,111,110,116,101,120,116,46,99,0
.global sgxConstantString38
sgxConstantString38:
	.byte 46,46,47,46,46,47,46,46,47,46,46,47,100,114,105,118,101,114,47,115,114,99,47,103,112,117,47,109,101,109,111,114,121,47,109,109,117,46,99,0
.global sgxConstantString39
sgxConstantString39:
	.byte 115,99,101,71,112,117,77,109,117,68,101,115,116,114,111,121,80,97,103,101,84,97,98,108,101,0
.global sgxConstantString40
sgxConstantString40:
	.byte 37,115,58,32,70,97,105,108,101,100,32,116,111,32,102,114,101,101,32,112,97,103,101,32,116,97,98,108,101,32,97,116,32,37,112,32,119,105,116,104,32,101,114,114,111,114,32,37,48,56,120,46,10,0
.align 2
.global sgxDebugInfo0
sgxDebugInfo0:
	.word 0xdeee73fd,0x27af3dc3,0xed9f42a
	.word sgxConstantString0
	.word 1265
.align 2
.global sgxDebugInfo1
sgxDebugInfo1:
	.word 0x9d2cccef,0x94fdbdf9,0xb59db67e
	.word sgxConstantString12
	.word 22
.align 2
.global sgxDebugInfo2
sgxDebugInfo2:
	.word 0x9d2cccef,0xc83069fa,0xb59db67e
	.word sgxConstantString12
	.word 28
.align 2
.global sgxDebugInfo3
sgxDebugInfo3:
	.word 0x9d2cccef,0x84e801a6,0xb59db67e
	.word sgxConstantString12
	.word 35
.align 2
.global sgxDebugInfo4
sgxDebugInfo4:
	.word 0x9d2cccef,0x213aab65,0xb59db67e
	.word sgxConstantString12
	.word 41
.align 2
.global sgxDebugInfo5
sgxDebugInfo5:
	.word 0x9d2cccef,0xf335d37d,0xb59db67e
	.word sgxConstantString12
	.word 47
.align 2
.global sgxDebugInfo6
sgxDebugInfo6:
	.word 0x9d2cccef,0x4b332fb1,0xb59db67e
	.word sgxConstantString12
	.word 53
.align 2
.global sgxDebugInfo7
sgxDebugInfo7:
	.word 0x9d2cccef,0x772f1178,0xb59db67e
	.word sgxConstantString12
	.word 60
.align 2
.global sgxDebugInfo8
sgxDebugInfo8:
	.word 0x9d2cccef,0xe63ea759,0xb59db67e
	.word sgxConstantString12
	.word 66
.align 2
.global sgxDebugInfo9
sgxDebugInfo9:
	.word 0x9d2cccef,0x5838a947,0xb59db67e
	.word sgxConstantString12
	.word 72
.align 2
.global sgxDebugInfo10
sgxDebugInfo10:
	.word 0x9d2cccef,0x11db6bc8,0xb59db67e
	.word sgxConstantString12
	.word 77
.align 2
.global sgxDebugInfo11
sgxDebugInfo11:
	.word 0x9d2cccef,0xd76726dc,0xb1f6e4d7
	.word sgxConstantString12
	.word 129
.align 2
.global sgxDebugInfo12
sgxDebugInfo12:
	.word 0x9d2cccef,0x1b12275,0xb1f6e4d7
	.word sgxConstantString12
	.word 137
.align 2
.global sgxDebugInfo13
sgxDebugInfo13:
	.word 0x2156171b,0x8f3117a8,0x6105f3ce
	.word sgxConstantString30
	.word 25
.align 2
.global sgxDebugInfo14
sgxDebugInfo14:
	.word 0x2156171b,0xb4e6d187,0x6105f3ce
	.word sgxConstantString30
	.word 32
.align 2
.global sgxDebugInfo15
sgxDebugInfo15:
	.word 0x2156171b,0x2eed9d0d,0x6105f3ce
	.word sgxConstantString30
	.word 38
.align 2
.global sgxDebugInfo16
sgxDebugInfo16:
	.word 0x2156171b,0x6cb82a7c,0x6105f3ce
	.word sgxConstantString30
	.word 44
.align 2
.global sgxDebugInfo17
sgxDebugInfo17:
	.word 0xc63ac79a,0xd535cfa8,0xa379d7a5
	.word sgxConstantString34
	.word 101
.align 2
.global sgxDebugInfo18
sgxDebugInfo18:
	.word 0x8df79155,0xbea39863,0x9bb9ab13
	.word sgxConstantString37
	.word 107
.align 2
.global sgxDebugInfo19
sgxDebugInfo19:
	.word 0x8df79155,0xfa600644,0x9bb9ab13
	.word sgxConstantString37
	.word 117
.align 2
.global sgxDebugInfo20
sgxDebugInfo20:
	.word 0x72c737da,0x86b36a6b,0xd178cf20
	.word sgxConstantString38
	.word 824
.align 2
.global sgxDebugInfo21
sgxDebugInfo21:
	.word 0x72c737da,0x7475e42f,0xd178cf20
	.word sgxConstantString38
	.word 830
.align 2
.global sgxDebugInfo22
sgxDebugInfo22:
	.word 0x72c737da,0x132d934e,0x6d821207
	.word sgxConstantString38
	.word 442
.align 2
.global sgxDebugInfo23
sgxDebugInfo23:
	.word 0x72c737da,0x8bdd4a3,0x6d821207
	.word sgxConstantString38
	.word 466
.align 2
.global sgxDebugInfo24
sgxDebugInfo24:
	.word 0x72c737da,0xf9a399cd,0xd178cf20
	.word sgxConstantString38
	.word 837
.align 2
.global sgxDebugInfo25
sgxDebugInfo25:
	.word 0x72c737da,0x15e91d09,0xdc5e2ca1
	.word sgxConstantString38
	.word 329
.align 2
.global sgxDebugInfo26
sgxDebugInfo26:
	.word 0x72c737da,0x879828c1,0xdc5e2ca1
	.word sgxConstantString38
	.word 330
.align 2
.global sgxDebugInfo27
sgxDebugInfo27:
	.word 0x72c737da,0x4ef4b046,0xdc5e2ca1
	.word sgxConstantString38
	.word 334
.bss
.align 2
.align 2
.global gpuConsoleLock
.type gpuConsoleLock, %object
gpuConsoleLock:
	.space 4
.size gpuConsoleLock,4
.align 2
.global gpuConsoleBuffer
.type gpuConsoleBuffer, %object
gpuConsoleBuffer:
	.space 513
.size gpuConsoleBuffer,513
.align 2
.global gpuLockupDevice
.type gpuLockupDevice, %object
gpuLockupDevice:
	.space 4
.size gpuLockupDevice,4
