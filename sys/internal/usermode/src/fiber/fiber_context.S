/*
 * SceFiber's context switch, hand-written in assembly by Sony (the file's name ours): what C
 * cannot say, setting sp and saving and loading the callee-saved registers.
 *
 * A fiber starts on its own stack (or the thread's, without a context area) with its entry's
 * two arguments; a suspended fiber or thread resumes where saveFiberContext returned, with a
 * nonzero return value. Either way the caller-saved registers are filled with poison values
 * first, the thread's stack limit (its TLS word pair, index 2) is copied from where the
 * fiber's is kept, and the Release forms then hand the fiber that was running back (its magic
 * set to ULT_FIBER_MAGIC_SAVED after a barrier). A fiber's entry does not return; if it does,
 * exitFiber ends it.
 *
 *	void startFiber(const FiberStartParam *param);
 *	void startFiberAndRelease(const FiberStartParam *param);
 *	int saveFiberContext(UltFiberContext *context);
 *	void restoreFiberContext(const UltFiberContext *context);
 *	void restoreFiberContextAndRelease(const UltFiberContext *context, SceUInt32 *magic);
 */

	.syntax unified
	.cpu cortex-a9
	.fpu neon
	.thumb

	.text

/* FiberStartParam */
#define START_ENTRY		0
#define START_ARG_ON_INITIALIZE	4
#define START_ARG_ON_RUN	8
#define START_SP		12
#define START_STACK_LIMIT	16
#define START_STACK_LIMIT_FROM	20
#define START_MAGIC		24

#define FIBER_MAGIC_SAVED	0x5a5a5a5a

	.p2align 2
	.global startFiber
	.type startFiber, %function
	.thumb_func
startFiber:
	mov ip, r0
	ldr.w r2, [ip, #START_STACK_LIMIT]
	ldr.w r3, [ip, #START_STACK_LIMIT_FROM]
	ldr.w sp, [ip, #START_SP]
	ldrd r0, r1, [r3]
	strd r0, r1, [r2]
	ldr.w r0, [ip, #START_ARG_ON_INITIALIZE]
	ldr.w r1, [ip, #START_ARG_ON_RUN]
	ldr.w ip, [ip, #START_ENTRY]
	/* the caller-saved registers hold nothing of the caller's: r0, r1 and ip are set */
.Lpoison:
	ldr r2, .Lpoison_word
	mov r3, r2
	vldr d0, .Lpoison_nan
	vmov d1, d0
	vmov q1, q0
	vmov q2, q0
	vmov q3, q0
	vmov q8, q0
	vmov q9, q0
	vmov q10, q0
	vmov q11, q0
	vmov q12, q0
	vmov q13, q0
	vmov q14, q0
	vmov q15, q0
	blx ip
	/* the entry returned */
	mov.w r0, #1
	b.w exitFiber
	.size startFiber, . - startFiber

	.p2align 2
	.global startFiberAndRelease
	.type startFiberAndRelease, %function
	.thumb_func
startFiberAndRelease:
	mov ip, r0
	ldr.w r2, [ip, #START_STACK_LIMIT]
	ldr.w r3, [ip, #START_STACK_LIMIT_FROM]
	ldr.w sp, [ip, #START_SP]
	ldrd r0, r1, [r3]
	strd r0, r1, [r2]
	ldr.w r2, [ip, #START_MAGIC]
	dmb sy
	mov.w r3, #FIBER_MAGIC_SAVED
	str r3, [r2]
	ldr.w r0, [ip, #START_ARG_ON_INITIALIZE]
	ldr.w r1, [ip, #START_ARG_ON_RUN]
	ldr.w ip, [ip, #START_ENTRY]
	b .Lpoison
	.size startFiberAndRelease, . - startFiberAndRelease

/*
 * The registers go to *context and the call returns 0; lr is then this file's
 * restoreFiberContext, which a return through the saved lr never reaches.
 */
	.p2align 2
	.global saveFiberContext
	.type saveFiberContext, %function
	.thumb_func
saveFiberContext:
	mov r1, sp
	stmia.w r0!, {r1, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	vstmia r0!, {d8-d15}
	fmrx ip, fpscr
	str.w ip, [r0], #4
	mov.w r0, #0
	blx lr
	.size saveFiberContext, . - saveFiberContext

/* the saved fpscr's flags are not restored: the current ones stay */
	.global restoreFiberContext
	.type restoreFiberContext, %function
	.thumb_func
restoreFiberContext:
	ldmia.w r0!, {r1, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	vldmia r0!, {d8-d15}
	ldr.w ip, [r0], #4
	mov sp, r1
	fmrx r0, fpscr
	movw r1, #0x00ff
	movt r1, #0xf800
	bic.w ip, ip, r1
	and.w r0, r0, r1
	orr.w ip, ip, r0
	fmxr fpscr, ip
	mov ip, lr
	ldr r0, .Lpoison_word
	mov r1, r0
	b .Lpoison
	.size restoreFiberContext, . - restoreFiberContext

	.p2align 2
	.global restoreFiberContextAndRelease
	.type restoreFiberContextAndRelease, %function
	.thumb_func
restoreFiberContextAndRelease:
	ldmia.w r0!, {r2, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	vldmia r0!, {d8-d15}
	ldr.w ip, [r0], #4
	mov sp, r2
	fmrx r0, fpscr
	movw r2, #0x00ff
	movt r2, #0xf800
	bic.w ip, ip, r2
	and.w r0, r0, r2
	orr.w ip, ip, r0
	fmxr fpscr, ip
	mov ip, lr
	dmb sy
	mov.w r0, #FIBER_MAGIC_SAVED
	str r0, [r1]
	ldr r0, .Lpoison_word
	mov r1, r0
	b .Lpoison
.Lpoison_word:
	.word 0xdeadbeef
.Lpoison_nan:
	.word 0x7ff8dead, 0x7f80dead
	.size restoreFiberContextAndRelease, . - restoreFiberContextAndRelease
