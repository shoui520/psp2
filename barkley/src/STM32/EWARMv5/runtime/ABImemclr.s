/*
 * __aeabi_memclr: n bytes cleared. __iar_Memset_word: n bytes set from a word-aligned
 * destination with the word in r2: 16 bytes at a time, then the rest.
 */
	.syntax	unified
	.thumb
	.text

	.global	__aeabi_memclr
	.type	__aeabi_memclr, %function
	.thumb_func
__aeabi_memclr:
	movs	r2, #0
	b.w	__iar_Memset_word
	.size	__aeabi_memclr, . - __aeabi_memclr

	.global	__iar_Memset_word
	.type	__iar_Memset_word, %function
	.thumb_func
__iar_Memset_word:
	push	{lr}
	movs	r3, r2
	mov	lr, r2
	mov	ip, r2
1:	subs	r1, #16
	it	cs
	stmiacs.w	r0!, {r2, r3, ip, lr}
	bhi	1b
	movs.w	ip, r1, lsl #29
	it	cs
	stmiacs	r0!, {r2, r3}
	it	mi
	strmi.w	r2, [r0], #4
	lsls	r1, r1, #31
	it	cs
	strhcs.w	r2, [r0], #2
	it	mi
	strbmi.w	r2, [r0], #1
	pop	{pc}
	.size	__iar_Memset_word, . - __iar_Memset_word
