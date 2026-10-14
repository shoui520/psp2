/* Native startup service dependencies. Private names remain descriptive. */
.syntax unified
.thumb
.text
.align 2
.global GpuProcessServicesDeinit
.type GpuProcessServicesDeinit, %function
.thumb_func
GpuProcessServicesDeinit:
	push {r3, lr}
	cbz r0, .LGpuProcessServicesDeinit10
	bl GlobalHandleBaseDeinit
	bl DeinitPerProcessHash
	bl ResourceManagerDeinit
.LGpuProcessServicesDeinit10:
	pop {r3, pc}
.size GpuProcessServicesDeinit, .-GpuProcessServicesDeinit
.align 2
.align 2
.global GpuProcessServicesInit
.type GpuProcessServicesInit, %function
.thumb_func
GpuProcessServicesInit:
	push {r3, r4, r5, lr}
	mov r5, r0
	bl ResourceManagerInit
	mov r4, r0
	cbnz r0, .LGpuProcessServicesInit3c
	bl InitPerProcessHash
	mov r4, r0
	cbnz r0, .LGpuProcessServicesInit3c
	bl GlobalHandleBaseInit
	mov r4, r0
	cbnz r0, .LGpuProcessServicesInit3c
	add.w r0, r5, #0x24
	bl GpuServicePairInit
	mov r4, r0
	cbnz r0, .LGpuProcessServicesInit3c
	mov.w r3, #-1
	strd r0, r3, [r5, #0x2c]
	ldr r3, GpuProcessServicesInitLiteral48
	ldr r2, [r3]
	orr r2, r2, #1
	str r2, [r3]
	b .LGpuProcessServicesInit42
.LGpuProcessServicesInit3c:
	mov r0, r5
	bl GpuProcessServicesDeinit
.LGpuProcessServicesInit42:
	mov r0, r4
	pop {r3, r4, r5, pc}
.size GpuProcessServicesInit, .-GpuProcessServicesInit
.align 2
GpuProcessServicesInitLiteral48:
	.word gpuServiceFlags
.align 2
.global GpuServicePairClearMatching
.type GpuServicePairClearMatching, %function
.thumb_func
GpuServicePairClearMatching:
	ldr r3, [r0]
	cbz r3, .LGpuServicePairClearMatching10
	ldr r3, [r0, #4]
	cmp r3, r1
	bne .LGpuServicePairClearMatching10
	movs r3, #0
	str r3, [r0, #4]
	str r3, [r0]
.LGpuServicePairClearMatching10:
	bx lr
.size GpuServicePairClearMatching, .-GpuServicePairClearMatching
.align 2
.align 2
.global GpuServicePairInit
.type GpuServicePairInit, %function
.thumb_func
GpuServicePairInit:
	movs r3, #0
	str r3, [r0, #4]
	str r3, [r0]
	mov r0, r3
	bx lr
.size GpuServicePairInit, .-GpuServicePairInit
.align 2
.align 2
.global GpuServicePairDeinit
.type GpuServicePairDeinit, %function
.thumb_func
GpuServicePairDeinit:
	push {r3, lr}
	ldr r1, [r0, #4]
	bl GpuServicePairClearMatching
	movs r0, #0
	pop {r3, pc}
.size GpuServicePairDeinit, .-GpuServicePairDeinit
.align 2
.align 2
.global GpuServiceBufferCreate
.type GpuServiceBufferCreate, %function
.thumb_func
GpuServiceBufferCreate:
	push {r3, r4, r5, lr}
	mov r5, r0
	movs r0, #0x28
	bl AllocProcessGpuMemory
	mov r4, r0
	cbz r0, .LGpuServiceBufferCreate20
	mov.w r0, #0x2000
	bl AllocProcessGpuMemory
	str r0, [r4]
	cbnz r0, .LGpuServiceBufferCreate24
	mov r0, r4
	bl FreeProcessGpuMemory
.LGpuServiceBufferCreate20:
	movs r0, #1
	pop {r3, r4, r5, pc}
.LGpuServiceBufferCreate24:
	movs r3, #0
	strd r3, r3, [r4, #4]
	mov r0, r3
	str r4, [r5]
	pop {r3, r4, r5, pc}
.size GpuServiceBufferCreate, .-GpuServiceBufferCreate
.align 2
.align 2
.global GpuServiceBufferDestroy
.type GpuServiceBufferDestroy, %function
.thumb_func
GpuServiceBufferDestroy:
	push {r3, r4, r5, lr}
	mov r4, r0
	movs r5, #0
	ldr r0, [r0]
	bl FreeProcessGpuMemory
	str r5, [r4]
	mov r0, r4
	bl FreeProcessGpuMemory
	mov r0, r5
	pop {r3, r4, r5, pc}
.size GpuServiceBufferDestroy, .-GpuServiceBufferDestroy
.align 2
.align 2
.global GpuResetControlWord
.type GpuResetControlWord, %function
.thumb_func
GpuResetControlWord:
	ldr r3, GpuResetControlWordLiteral8
	movs r2, #0
	str r2, [r3]
	bx lr
.size GpuResetControlWord, .-GpuResetControlWord
.align 2
GpuResetControlWordLiteral8:
	.word gpuResetControlWord
.align 2
.global CreateRegisterDumpMutex
.type CreateRegisterDumpMutex, %function
.thumb_func
CreateRegisterDumpMutex:
	movs r1, #0
	push {r3, lr}
	mov r2, r1
	mov r3, r1
	ldr r0, CreateRegisterDumpMutexLiteral14
	bl ksceKernelCreateMutex
	ldr r3, CreateRegisterDumpMutexLiteral18
	str r0, [r3]
	pop {r3, pc}
.size CreateRegisterDumpMutex, .-CreateRegisterDumpMutex
.align 2
CreateRegisterDumpMutexLiteral14:
	.word gpuRegisterDumpMutexName
CreateRegisterDumpMutexLiteral18:
	.word registerDumpMutex
.align 2
.global GpuResetSuspendWord
.type GpuResetSuspendWord, %function
.thumb_func
GpuResetSuspendWord:
	ldr r3, GpuResetSuspendWordLiteral8
	movs r0, #0
	str r0, [r3]
	bx lr
.size GpuResetSuspendWord, .-GpuResetSuspendWord
.align 2
GpuResetSuspendWordLiteral8:
	.word gpuSuspendWord
.align 2
.global CreateGpuSuspendWaitEvent
.type CreateGpuSuspendWaitEvent, %function
.thumb_func
CreateGpuSuspendWaitEvent:
	movs r2, #0
	push {r3, lr}
	ldr r0, CreateGpuSuspendWaitEventLiteral1c
	mov.w r1, #0x9000
	mov r3, r2
	bl ksceKernelCreateEventFlag
	cmp r0, #0
	blt .LCreateGpuSuspendWaitEvent1a
	ldr r3, CreateGpuSuspendWaitEventLiteral20
	str r0, [r3]
	movs r0, #0
.LCreateGpuSuspendWaitEvent1a:
	pop {r3, pc}
.size CreateGpuSuspendWaitEvent, .-CreateGpuSuspendWaitEvent
.align 2
CreateGpuSuspendWaitEventLiteral1c:
	.word gpuSuspendWaitEventName
CreateGpuSuspendWaitEventLiteral20:
	.word gpuSuspendWaitEvent
.data
.align 2
.global gpuServiceFlags
gpuServiceFlags: .word 0
.global gpuResetControlWord
gpuResetControlWord: .word 0
.global gpuSuspendWord
gpuSuspendWord: .word 0
.global gpuSuspendWaitEvent
gpuSuspendWaitEvent: .word 0
.section .rodata
.global gpuSuspendWaitEventName
gpuSuspendWaitEventName: .asciz "SceGpuSuspendWait"
.global gpuRegisterDumpMutexName
gpuRegisterDumpMutexName: .asciz "SceGpuCleanupMutex"
