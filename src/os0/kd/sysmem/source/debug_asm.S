/*
 * SceSysmem: the two pieces of the fault path C cannot express. Not Sony's
 * names (unknown).
 */

	.syntax unified
	.arch armv7-a
	.arch_extension sec
	.thumb
	.text

/*
 * void DebugCallOnStack(void *arg, void *stackTop, void (*func)(void *)):
 * func(arg) on another stack (the fault dumps run on a per-CPU stack)
 */
	.balign 4
	.global DebugCallOnStack
	.type DebugCallOnStack, %function
	.thumb_func
DebugCallOnStack:
	push.w	{r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	mov	r4, sp
	mov	sp, r1
	blx	r2
	mov	sp, r4
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	.size DebugCallOnStack, . - DebugCallOnStack

/*
 * void DebugSecureCrashDump(int kind, SceUInt32 ttbr0, SceUInt32 vbar, void *sysroot):
 * secure monitor service 0x122, the secure world's side of a crash dump
 */
	.global DebugSecureCrashDump
	.type DebugSecureCrashDump, %function
	.thumb_func
DebugSecureCrashDump:
	movw	ip, #0x122
	smc	#0
	bx	lr
	.size DebugSecureCrashDump, . - DebugSecureCrashDump
