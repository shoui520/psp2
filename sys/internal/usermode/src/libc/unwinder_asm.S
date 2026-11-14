/*
 * unwinder_asm.S: what the unwinder (unwinder.c) cannot write in C. The VFP registers are
 * saved and restored as whole banks; a landing pad is entered by loading every core
 * register of the virtual register set; and the entry points capture their caller's core
 * registers as a phase2_virtual_register_set on the stack:
 *
 *	sp+4	0 (demand_save_vfp_low, demand_save_vfp_high: nothing to save)
 *	sp+8	r0-r12
 *	sp+60	sp at the call
 *	sp+64	lr
 *	sp+68	pc (filled in from lr by the C)
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.macro	function name
	.global	\name
	.type	\name, %function
	.thumb_func
\name:
	.endm

/* void __ARM_Unwind_VRS_VFPpreserve_low(void *vfpp): store d0-d15 */
function __ARM_Unwind_VRS_VFPpreserve_low
	vstmia	r0, {d0-d15}
	bx	lr
	.size	__ARM_Unwind_VRS_VFPpreserve_low, . - __ARM_Unwind_VRS_VFPpreserve_low

/* void __ARM_Unwind_VRS_VFPpreserve_high(void *vfpp): store d16-d31 */
function __ARM_Unwind_VRS_VFPpreserve_high
	vstmia	r0, {d16-d31}
	bx	lr
	.size	__ARM_Unwind_VRS_VFPpreserve_high, . - __ARM_Unwind_VRS_VFPpreserve_high

/* void __ARM_Unwind_VRS_VFPrestore_low(void *vfpp): load d0-d15 */
function __ARM_Unwind_VRS_VFPrestore_low
	vldmia	r0, {d0-d15}
	bx	lr
	.size	__ARM_Unwind_VRS_VFPrestore_low, . - __ARM_Unwind_VRS_VFPrestore_low

/* void __ARM_Unwind_VRS_VFPrestore_high(void *vfpp): load d16-d31 */
function __ARM_Unwind_VRS_VFPrestore_high
	vldmia	r0, {d16-d31}
	bx	lr
	.size	__ARM_Unwind_VRS_VFPrestore_high, . - __ARM_Unwind_VRS_VFPrestore_high

/*
 * void __ARM_Unwind_VRS_corerestore(void *corep): load r0-r15 from corep. lr and pc go
 * onto the new stack first, and come off it last.
 */
function __ARM_Unwind_VRS_corerestore
	ldr.w	lr, [r0, #0x34]		/* the new sp */
	add.w	r1, r0, #0x38
	ldmia.w	r1, {r2, r3}		/* the new lr and pc */
	stmdb.w	lr!, {r2, r3}
	ldmia.w	r0, {r0-r12}
	mov	sp, lr
	ldr.w	lr, [sp], #4
	pop	{pc}
	.size	__ARM_Unwind_VRS_corerestore, . - __ARM_Unwind_VRS_corerestore

/* capture the caller's registers, call the C, and return to the caller if it returns */
	.macro	entry name, c, lr_slot
function \name
	str.w	lr, [sp, #-8]!
	add.w	lr, sp, #8
	str.w	lr, [sp, #-4]!
	stmdb.w	sp!, {r0-r12}
	mov.w	r1, #0
	str.w	r1, [sp, #-4]!
	mov	r1, sp
	sub	sp, #4
	bl	\c
	ldr.w	lr, [sp, #\lr_slot]
	add	sp, #0x48
	bx	lr
	.size	\name, . - \name
	.endm

/* _Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Control_Block *ucbp) */
	entry	_Unwind_RaiseException, __ARM_Unwind_RaiseException, 0x40

/*
 * void __cxa_end_cleanup(void): the end of a cleanup landing pad. __ARM_cxa_end_cleanup
 * gives back the exception's control block, and the cleanup's registers go with it into
 * _Unwind_Resume.
 */
function __cxa_end_cleanup
	stmdb.w	sp!, {r1-r12}
	bl	__ARM_cxa_end_cleanup
	ldmia.w	sp!, {r1-r12}
	.size	__cxa_end_cleanup, . - __cxa_end_cleanup

/*
 * void _Unwind_Resume(_Unwind_Control_Block *ucbp): the C never returns; Sony's reload of
 * lr reads the r10 slot (sp+0x30), as in _Unwind_Walk.
 */
	entry	_Unwind_Resume, __ARM_Unwind_Resume, 0x30

/* void _Unwind_Walk(void): unwind the caller's stack virtually (name ours) */
	entry	_Unwind_Walk, __ARM_Unwind_Walk, 0x30

/* void *__ARM_Unwind_ReturnAddress(void): the caller's return address (name ours) */
function __ARM_Unwind_ReturnAddress
	mov	r0, lr
	bx	lr
	.size	__ARM_Unwind_ReturnAddress, . - __ARM_Unwind_ReturnAddress
