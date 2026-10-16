/* Native startup service dependencies. Private names remain descriptive. */
.syntax unified
.thumb
.text
.align 2
.global GpuDeviceIndexAllocate
.type GpuDeviceIndexAllocate, %function
.thumb_func
GpuDeviceIndexAllocate:
	ldr r2, [r0]
	adds r3, r0, #4
	add.w r2, r3, r2, lsl #3
.LGpuDeviceIndexAllocate8:
	cmp r3, r2
	bhs .LGpuDeviceIndexAllocate1e
	ldr r0, [r3, #4]
	cbnz r0, .LGpuDeviceIndexAllocate1a
	movs r2, #1
	str r2, [r3, #4]
	ldr r3, [r3]
	str r3, [r1]
	bx lr
.LGpuDeviceIndexAllocate1a:
	adds r3, #8
	b .LGpuDeviceIndexAllocate8
.LGpuDeviceIndexAllocate1e:
	movs r0, #0xa3
	bx lr
.size GpuDeviceIndexAllocate, .-GpuDeviceIndexAllocate
.align 2
.align 2
.global GpuDeviceIndexRelease
.type GpuDeviceIndexRelease, %function
.thumb_func
GpuDeviceIndexRelease:
	adds r3, r0, #4
	ldr r0, [r0]
	add.w r0, r3, r0, lsl #3
.LGpuDeviceIndexRelease8:
	cmp r3, r0
	bhs .LGpuDeviceIndexRelease20
	ldr r2, [r3]
	cmp r2, r1
	bne .LGpuDeviceIndexRelease1c
	ldr r2, [r3, #4]
	cbz r2, .LGpuDeviceIndexRelease1c
	movs r0, #0
	str r0, [r3, #4]
	bx lr
.LGpuDeviceIndexRelease1c:
	adds r3, #8
	b .LGpuDeviceIndexRelease8
.LGpuDeviceIndexRelease20:
	movs r0, #0xa4
	bx lr
.size GpuDeviceIndexRelease, .-GpuDeviceIndexRelease
.align 2
.align 2
.global GpuDeviceListRunUntilError
.type GpuDeviceListRunUntilError, %function
.thumb_func
GpuDeviceListRunUntilError:
	push {r3, r4, r5, lr}
	mov r3, r0
	mov r4, r1
	movs r0, #0
.LGpuDeviceListRunUntilError8:
	cbz r3, .LGpuDeviceListRunUntilError16
	cbnz r0, .LGpuDeviceListRunUntilError16
	ldr r5, [r3, #0x68]
	mov r0, r3
	blx r4
	mov r3, r5
	b .LGpuDeviceListRunUntilError8
.LGpuDeviceListRunUntilError16:
	pop {r3, r4, r5, pc}
.size GpuDeviceListRunUntilError, .-GpuDeviceListRunUntilError
.align 2
.align 2
.global GpuDeviceListFindVariadic
.type GpuDeviceListFindVariadic, %function
.thumb_func
GpuDeviceListFindVariadic:
	push {r1, r2, r3}
	mov r3, r0
	push {r0, r1, r2, r4, r5, r6, lr}
	movs r0, #0
	ldr r2, GpuDeviceListFindVariadicLiteral3c
	ldr r5, [sp, #0x1c]
	ldr r1, [r2]
	mov r4, r2
	str r1, [sp, #4]
.LGpuDeviceListFindVariadic12:
	cbnz r3, .LGpuDeviceListFindVariadic20
.LGpuDeviceListFindVariadic14:
	ldr r2, [sp, #4]
	ldr r3, [r4]
	cmp r2, r3
	beq .LGpuDeviceListFindVariadic32
	bl __stack_chk_fail
.LGpuDeviceListFindVariadic20:
	cmp r0, #0
	bne .LGpuDeviceListFindVariadic14
	ldr r6, [r3, #0x68]
	add r1, sp, #0x20
	mov r0, r3
	str r1, [sp]
	blx r5
	mov r3, r6
	b .LGpuDeviceListFindVariadic12
.LGpuDeviceListFindVariadic32:
	add sp, #0xc
	pop.w {r4, r5, r6, lr}
	add sp, #0xc
	bx lr
.size GpuDeviceListFindVariadic, .-GpuDeviceListFindVariadic
.align 2
GpuDeviceListFindVariadicLiteral3c:
	.word __stack_chk_guard
.align 2
.global GpuDeviceListForEach
.type GpuDeviceListForEach, %function
.thumb_func
GpuDeviceListForEach:
	push {r3, r4, r5, lr}
	mov r4, r0
	mov r5, r1
.LGpuDeviceListForEach6:
	cbz r4, .LGpuDeviceListForEach10
	mov r0, r4
	blx r5
	ldr r4, [r4, #0x68]
	b .LGpuDeviceListForEach6
.LGpuDeviceListForEach10:
	pop {r3, r4, r5, pc}
.size GpuDeviceListForEach, .-GpuDeviceListForEach
.align 2
.align 2
.global GpuDeviceListForEachVariadic
.type GpuDeviceListForEachVariadic, %function
.thumb_func
GpuDeviceListForEachVariadic:
	push {r1, r2, r3}
	ldr r3, GpuDeviceListForEachVariadicLiteral34
	push {r0, r1, r2, r4, r5, r6, lr}
	mov r4, r0
	ldr r6, [sp, #0x1c]
	mov r5, r3
	ldr r2, [r3]
	str r2, [sp, #4]
.LGpuDeviceListForEachVariadic10:
	cbz r4, .LGpuDeviceListForEachVariadic1e
	add r1, sp, #0x20
	mov r0, r4
	str r1, [sp]
	blx r6
	ldr r4, [r4, #0x68]
	b .LGpuDeviceListForEachVariadic10
.LGpuDeviceListForEachVariadic1e:
	ldr r2, [sp, #4]
	ldr r3, [r5]
	cmp r2, r3
	beq .LGpuDeviceListForEachVariadic2a
	bl __stack_chk_fail
.LGpuDeviceListForEachVariadic2a:
	add sp, #0xc
	pop.w {r4, r5, r6, lr}
	add sp, #0xc
	bx lr
.size GpuDeviceListForEachVariadic, .-GpuDeviceListForEachVariadic
.align 2
GpuDeviceListForEachVariadicLiteral34:
	.word __stack_chk_guard
.align 2
.global GpuDeviceListInsert
.type GpuDeviceListInsert, %function
.thumb_func
GpuDeviceListInsert:
	str r0, [r1, #0x6c]
	ldr r3, [r0]
	str r3, [r1, #0x68]
	str r1, [r0]
	ldr r3, [r1, #0x68]
	cbz r3, .LGpuDeviceListInsert10
	adds r1, #0x68
	str r1, [r3, #0x6c]
.LGpuDeviceListInsert10:
	bx lr
.size GpuDeviceListInsert, .-GpuDeviceListInsert
.align 2
.align 2
.global GpuDeviceListUnlink
.type GpuDeviceListUnlink, %function
.thumb_func
GpuDeviceListUnlink:
	ldrd r2, r3, [r0, #0x68]
	str r2, [r3]
	ldr r3, [r0, #0x68]
	cbz r3, .LGpuDeviceListUnlinke
	ldr r2, [r0, #0x6c]
	str r2, [r3, #0x6c]
.LGpuDeviceListUnlinke:
	bx lr
.size GpuDeviceListUnlink, .-GpuDeviceListUnlink
.align 2
.align 2
.global GpuDeviceMatchIndexClass
.type GpuDeviceMatchIndexClass, %function
.thumb_func
GpuDeviceMatchIndexClass:
	push {r4, lr}
	ldrd r4, r3, [r1]
	cbnz r3, .LGpuDeviceMatchIndexClass10
	ldr r2, [r0, #4]
	ldr r1, [r1, #8]
	cmp r2, r1
	bne .LGpuDeviceMatchIndexClass1a
.LGpuDeviceMatchIndexClass10:
	ldr r3, [r0, #8]
	cmp r3, r4
	it ne
	movne r0, #0
	pop {r4, pc}
.LGpuDeviceMatchIndexClass1a:
	mov r0, r3
	pop {r4, pc}
.size GpuDeviceMatchIndexClass, .-GpuDeviceMatchIndexClass
.align 2
.align 2
.global GpuDeviceMatchFlags
.type GpuDeviceMatchFlags, %function
.thumb_func
GpuDeviceMatchFlags:
	ldr r3, [r0, #0x14]
	ldr r2, [r1]
	cmp r3, r2
	it ne
	movne r0, #0
	bx lr
.size GpuDeviceMatchFlags, .-GpuDeviceMatchFlags
.align 2
.align 2
.global GpuRegisterDevice
.type GpuRegisterDevice, %function
.thumb_func
GpuRegisterDevice:
	push.w {r0, r1, r2, r4, r5, r6, r7, r8, sb, lr}
	mov r8, r3
	ldr r6, GpuRegisterDeviceLiteral84
	mov r4, r0
	mov r7, r1
	mov sb, r2
	movs r0, #2
	movs r1, #0x70
	ldr r3, [r6]
	mov r2, sp
	str r3, [sp, #4]
	movs r3, #0
	bl GpuAllocFlags
	mov r5, r0
	cbnz r0, .LGpuRegisterDevice6e
	ldr r0, [sp]
	mov r1, r5
	movs r2, #0x70
	bl memset
	ldr r0, [sp]
	blx r7
	mov r7, r0
	cbz r0, .LGpuRegisterDevice44
	movs r0, #2
	movs r1, #0x70
	ldr r2, [sp]
	mov r3, r5
	bl GpuFreeFlags
	movs r0, #0x11
	b .LGpuRegisterDevice70
.LGpuRegisterDevice44:
	ldr r1, [sp]
	movs r3, #1
	mov r0, r4
	str r3, [r1, #0x14]
	adds r1, #8
	str r4, [r1, #0x58]
	str.w sb, [r1, #0x28]
	bl GpuDeviceIndexAllocate
	add.w r0, r4, #0x1c
	ldr r1, [sp]
	bl GpuDeviceListInsert
	ldr r3, [sp]
	mov r0, r7
	ldr r3, [r3, #8]
	str.w r3, [r8]
	b .LGpuRegisterDevice70
.LGpuRegisterDevice6e:
	movs r0, #1
.LGpuRegisterDevice70:
	ldr r2, [sp, #4]
	ldr r3, [r6]
	cmp r2, r3
	beq .LGpuRegisterDevice7c
	bl __stack_chk_fail
.LGpuRegisterDevice7c:
	add sp, #0xc
	pop.w {r4, r5, r6, r7, r8, sb, pc}
	nop 
.size GpuRegisterDevice, .-GpuRegisterDevice
.align 2
GpuRegisterDeviceLiteral84:
	.word __stack_chk_guard
.align 2
.global GpuInitialiseDeviceResource
.type GpuInitialiseDeviceResource, %function
.thumb_func
GpuInitialiseDeviceResource:
	ldr r3, GpuInitialiseDeviceResourceLiteral34
	mov r2, r0
	push {r4, lr}
	ldr r1, GpuInitialiseDeviceResourceLiteral38
	ldr r3, [r3]
	ldr r0, [r3, #0x1c]
	movs r3, #1
	bl GpuDeviceListFindVariadic
	mov r4, r0
	cbz r0, .LGpuInitialiseDeviceResource2c
	movs r0, #0
	add.w r1, r4, #0x5c
	bl ResourceContextCreate
	cbnz r0, .LGpuInitialiseDeviceResource32
	ldr r3, [r4, #0x18]
	cbz r3, .LGpuInitialiseDeviceResource30
	mov r0, r4
	blx r3
	pop {r4, pc}
.LGpuInitialiseDeviceResource2c:
	movs r0, #4
	pop {r4, pc}
.LGpuInitialiseDeviceResource30:
	mov r0, r3
.LGpuInitialiseDeviceResource32:
	pop {r4, pc}
.size GpuInitialiseDeviceResource, .-GpuInitialiseDeviceResource
.align 2
GpuInitialiseDeviceResourceLiteral34:
	.word gpuDumpRegion + 0x3178
GpuInitialiseDeviceResourceLiteral38:
	.word GpuDeviceMatchIndexClass
.align 2
.global GpuInvokeDeviceCallback
.type GpuInvokeDeviceCallback, %function
.thumb_func
GpuInvokeDeviceCallback:
	push {r3, lr}
	ldr r3, [r0, #0x20]
	cbz r3, .LGpuInvokeDeviceCallbacka
	blx r3
	pop {r3, pc}
.LGpuInvokeDeviceCallbacka:
	mov r0, r3
	pop {r3, pc}
.size GpuInvokeDeviceCallback, .-GpuInvokeDeviceCallback
.align 2
.align 2
.global GpuInvokeDeviceCallbackWrapper
.type GpuInvokeDeviceCallbackWrapper, %function
.thumb_func
GpuInvokeDeviceCallbackWrapper:
	push {r3, lr}
	bl GpuInvokeDeviceCallback
	pop {r3, pc}
.size GpuInvokeDeviceCallbackWrapper, .-GpuInvokeDeviceCallbackWrapper
.align 2
