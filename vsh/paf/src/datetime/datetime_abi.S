/* Preserve native parser state that is outside the declared C++ arguments. */
	.syntax	unified
	.thumb
	.text

	.global	ScePafMisc_900114B8
	.type	ScePafMisc_900114B8, %function
	.thumb_func
ScePafMisc_900114B8:
	.fnstart
	mov	r2, r5
	b.w	DateTimeParseSQLiteDateTimeBody
	.fnend
	.size	ScePafMisc_900114B8, . - ScePafMisc_900114B8

/* Expose strlen's r0/r1 pair as a C uint64_t, without another call frame. */
	.global	DateTimeStringLengthState
	.type	DateTimeStringLengthState, %function
	.thumb_func
DateTimeStringLengthState:
	.fnstart
	b.w	sce_paf_strlen
	.fnend
	.size	DateTimeStringLengthState, . - DateTimeStringLengthState
