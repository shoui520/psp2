/* Native allocator flag wrapper frames; remote allocation policy is shared. */
.syntax unified
.thumb
.text
.align 2
.global GpuAllocFlags
.type GpuAllocFlags, %function
.thumb_func
GpuAllocFlags:
	push {r4, lr}
	mov r0, r1
	mov r4, r2
	bl AllocProcessGpuMemory
	str r0, [r4]
	clz r0, r0
	lsrs r0, r0, #5
	pop {r4, pc}
.size GpuAllocFlags, .-GpuAllocFlags
.global GpuFreeFlags
.type GpuFreeFlags, %function
.thumb_func
GpuFreeFlags:
	push {r3, lr}
	mov r0, r2
	bl FreeProcessGpuMemory
	movs r0, #0
	pop {r3, pc}
.size GpuFreeFlags, .-GpuFreeFlags

.global GetRemoteAllocProcess
.type GetRemoteAllocProcess, %function
.thumb_func
GetRemoteAllocProcess:
	ldr r3, =remoteAllocProcess
	ldr r0, [r3]
	bx lr
.size GetRemoteAllocProcess, .-GetRemoteAllocProcess
