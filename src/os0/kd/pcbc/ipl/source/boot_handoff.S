/* Enter a kernel boot function on its supplied stack. C cannot replace SP. */
	.set	noreorder
	.text
	.align	2
	.globl	callWithBootStack
	.type	callWithBootStack, @function
	.ent	callWithBootStack
callWithBootStack:
	addu	$v1, $a0, $zero
	addu	$v0, $a1, $zero
	addiu	$a3, $a3, -64
	addu	$a0, $v0, $zero
	addu	$a1, $a2, $zero
	jr	$v1
	addu	$sp, $a3, $zero
	.end	callWithBootStack
	.size	callWithBootStack, . - callWithBootStack
