/*
 * Syscall handler of every traced function (registered by
 * sceSyslibtraceStartSyscallTrace). The enter hook replaces the saved lr with
 * the traced function, which is then called with the caller's r0-r3/ip; the
 * return hook puts the caller's lr back. Assembly: the hooks rewrite the saved
 * register frame, and the traced function must see the untouched argument
 * registers.
 */
	.syntax	unified
	.thumb
	.text

	.global	SyslibtraceSyscallTrampoline
	.type	SyslibtraceSyscallTrampoline, %function
	.thumb_func
SyslibtraceSyscallTrampoline:
	push.w	{r0, r1, r2, r3, ip, lr}
	mov	r0, sp
	movw	lr, #:lower16:SceSyslibtraceSyscallEnterHook
	movt	lr, #:upper16:SceSyslibtraceSyscallEnterHook
	blx	lr
	pop.w	{r0, r1, r2, r3, ip, lr}
	blx	lr				@ the traced function
	push.w	{r0, r1, r2, r3, ip, lr}
	mov	r0, sp
	movw	lr, #:lower16:SceSyslibtraceSyscallReturnHook
	movt	lr, #:upper16:SceSyslibtraceSyscallReturnHook
	blx	lr
	pop.w	{r0, r1, r2, r3, ip, lr}
	bx	lr
	.size	SyslibtraceSyscallTrampoline, . - SyslibtraceSyscallTrampoline
